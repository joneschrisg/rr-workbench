#define _GNU_SOURCE

#include <err.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

//#define ENABLE_PTRACE
#define ENABLE_INTERRUPT
#define ENABLE_HANDLE_INTERRUPT
/* When ptrace is enabled and we don't operate one-shot, we get into
 * an infinite loop of de-sched-interrupt, notify ptrace, sighandler,
 * de-sched interrupt, notify ptrace, ...  We need to have the parent
 * disarm the counter to avoid this. */
//#define ONESHOT_INTERRUPT

static const size_t nr_switches = 1, cpu_ticks = 1 << 20, io_ticks = 20;
static const enum { CPU, IO } which = IO;

static int cs_fd, desched_counter;

static int
sys_gettid()
{
    return syscall(SYS_gettid);
}

static int
sys_perf_event_open(struct perf_event_attr *attr,
                    pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    return syscall(SYS_perf_event_open, attr, pid, cpu, group_fd, flags);
}

static int
open_cs_counter(size_t nr_switches)
{
    struct perf_event_attr attr;
    int fd;
    struct f_owner_ex own;

    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.type = PERF_TYPE_SOFTWARE;
    attr.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
    attr.disabled = 1;
    attr.sample_period = nr_switches;

    fd = sys_perf_event_open(&attr, 0/*self*/, -1/*any cpu*/, -1, 0);
    if (0 > fd)
        err(1, "perf_event_open(cs, period=%u)", nr_switches);

    if (fcntl(fd, F_SETFL, O_ASYNC))
        err(1, "fcntl(O_ASYNC)");

    own.type = F_OWNER_TID;
    own.pid = sys_gettid();
    if (fcntl(fd, F_SETOWN_EX, &own))
        err(1, "fcntl(SETOWN_EX)");

    if (fcntl(fd, F_SETSIG, SIGIO))
        err(1, "fcntl(SETSIG, SIGIO)");

    return fd;
}

static void
chew_cpu(size_t ticks)
{
    size_t i;

    for (i = 0; i < ticks; ++i) {
        if (0 == i % 10) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
        } else {
            int j = i * (i / 31 + i / 77) % (i + 23);
            (void)j;
        }
    }
}

static void
spam_io(size_t ticks)
{
    size_t i;
    int fd;

#if 1
    /* With this, we get |ticks| de-sched interrupts.  Scheduler seems
     * to tune for tty to be very interactive. */
    fd = STDOUT_FILENO;
#else
    /* With this, we get 0 de-scheds.  That's more like what one would
     * expect.*/
    fd = open("/tmp/cs_signal.foo", O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif

    for (i = 0; i < ticks - 1; ++i) {
        write(fd, ".", 1);
    }
    write(fd, "\n", 1);
}

static void
clear_counter(int signo, siginfo_t* si, void* ctx)
{
    uint64_t nr_switch;

#ifdef ONESHOT_INTERRUPT
    if (ioctl(cs_fd, PERF_EVENT_IOC_DISABLE, 0))
        err(1, "ioctl(COUNTER_DISABLE)");
#endif

    if (sizeof(nr_switch) != read(si->si_fd, &nr_switch, sizeof(nr_switch)))
        err(1, "read(counterfd)");

    ++desched_counter;
}

static void
child()
{
    struct sigaction act;

    printf("child: execution info:\n"
           "  child() = %p\n"
           "  nr_switches = %u\n",
           child,
           nr_switches);
    if (IO == which) {
        printf("  spam_io() = %p, %u ticks\n", spam_io, io_ticks);
    } else {
        printf("  chew_cpu() = %p, %u ticks\n", chew_cpu, cpu_ticks);
    }

    memset(&act, 0, sizeof(act));
    act.sa_sigaction = clear_counter;
    act.sa_flags = SA_SIGINFO;
#ifdef ENABLE_HANDLE_INTERRUPT
    sigaction(SIGIO, &act, NULL);
#endif

    cs_fd = open_cs_counter(nr_switches);

#ifdef ENABLE_PTRACE
    puts("child accepting ptrace ...");
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    raise(SIGSTOP);
#endif

#ifdef ENABLE_INTERRUPT
    if (ioctl(cs_fd, PERF_EVENT_IOC_ENABLE, 0))
        err(1, "ioctl(PERF_EVENT_ENABLE)");
#endif

//    chew_cpu(cpu_ticks);
    spam_io(io_ticks);
//    *((volatile int*)0) = 42;

    printf("child: done; caught %d de-sched interrupts\n", desched_counter);
    fflush(stdout);

    _exit(0);
}

static int
dump_status(pid_t p, int status)
{
    if (WIFSTOPPED(status)) {
        printf("%d stopped by signal %d\n", p, WSTOPSIG(status));
        return 1;
    } else if (WIFCONTINUED(status)) {
        printf("%d continued by SIGCONT\n", p);
        return 1;
    } else if (WIFEXITED(status)) {
        printf("%d exited with code %d\n", p, WEXITSTATUS(status));
        return 0;
    } else if (WIFSIGNALED(status)) {
        printf("%d terminated by signal %d\n", p, WTERMSIG(status));
        return 0;
    } else {
        printf("%d seems to be running normally\n", p);
        return 1;
    }
}

pid_t p;

static void
parent()
{
    while (1) {
        int status;

        waitpid(p, &status, 0);
        if (!dump_status(p, status)) {
            return;
        }

#ifdef ENABLE_PTRACE
        int contsig = 0;
        if (WIFSTOPPED(status)) {
            struct siginfo si;
            struct user_regs_struct regs;

            ptrace(PTRACE_GETSIGINFO, p, 0, &si);
            ptrace(PTRACE_GETREGS, p, 0, &regs);
            printf(" ... at %p (fd %d)\n", (void*)regs.eip, si.si_fd);
            contsig = si.si_signo;
        }

        ptrace(PTRACE_CONT, p, 0, contsig);
#endif
    }
}

int
main(int argc, char** argv)
{
    if (0 == (p = fork())) {
        child();
    } else {
        parent();
    }
    return 0;
}
