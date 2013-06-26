#include <err.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <signal.h>
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

pid_t p;

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

    memset(&attr, 0, sizeof(attr));
    attr.size = sizeof(attr);
    attr.type = PERF_TYPE_SOFTWARE;
    attr.config = PERF_COUNT_SW_CONTEXT_SWITCHES;
    attr.disabled = 1;
    attr.sample_period = nr_switches;

    fd = sys_perf_event_open(&attr, 0/*self*/, -1/*any cpu*/, -1, 0);
    if (0 > fd)
        err(1, "perf_event_open(cs, period=%u)", nr_switches);

    if (fcntl(fd, F_SETOWN, getpid()))
        err(1, "fcntl(SETOWN)");

    if (fcntl(fd, F_SETFL, O_ASYNC))
        err(1, "fcntl(O_ASYNC)");

    return fd;
}

static void
chew_cpu(size_t ticks)
{
    size_t i;

    for (i = 0; i < ticks; ++i) {
        write(-1, NULL, 0);
        if (i && !(i % 1024))
            write(STDOUT_FILENO, ".", 1);
    }
}

static void
child()
{
    size_t nr_switches = 1, ticks = 1 << 11;

    int fd;
    struct timeval tv;

    printf("symbol info: chew_cpu() = %p, child() = %p\n", chew_cpu, child);

    ptrace(PTRACE_TRACEME, 0, 0, 0);
    kill(getpid(), SIGSTOP);

    fd = open_cs_counter(nr_switches);
    printf("child opened cs counter for %u switches\n", nr_switches);

    printf("child chewing %u syscall ticks ...\n", ticks);

    if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0))
        err(1, "ioctl(PERF_EVENT_ENABLE)");

    //gettimeofday(&tv, NULL);
    (void)tv;
    chew_cpu(ticks);

    _exit(0);
}

static void
dump_status(pid_t p, int status)
{
    if (WIFSTOPPED(status)) {
        printf("%d stopped by signal %d\n", p, WSTOPSIG(status));
    } else if (WIFCONTINUED(status)) {
        printf("%d continued by SIGCONT\n", p);
    } else if (WIFEXITED(status)) {
        printf("%d exited with code %d\n", p, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("%d terminated by signal %d\n", p, WTERMSIG(status));
    } else {
        printf("%d seems to be running normally\n", p);
    }
}

static void
parent()
{
    int status;
    struct siginfo si;
    struct user_regs_struct regs;

    waitpid(p, &status, 0);
    dump_status(p, status);

    ptrace(PTRACE_CONT, p, 0, 0);

    waitpid(p, &status, 0);
    dump_status(p, status);

    if (WIFSIGNALED(status)) {
        ptrace(PTRACE_GETSIGINFO, p, 0, &si);
        ptrace(PTRACE_GETREGS, p, 0, &regs);
        printf("%d stopped by signal %d at %p\n", p, si.si_signo, (void*)regs.eip);
        sleep(30);
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
