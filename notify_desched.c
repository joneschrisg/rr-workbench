#define _GNU_SOURCE

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#if 1
# define debug(msg, ...) printf(msg "\n", ##__VA_ARGS__)
#else
# define debug(...) ((void)0)
#endif

static const enum { CPU, IO } which = IO;
static const size_t nr_switches = 1, cpu_ticks = 1 << 25, io_ticks = 10;

/*-----------------------------------------------------------------------------
 * Shared
 */
static int parent_socket, child_socket;

static void
send_fd(int fd, int socket)
{
    struct msghdr msg;
    struct iovec data;
    struct cmsghdr* cmsg;
    char cmsgbuf[CMSG_SPACE(sizeof(int))];

    memset(&msg, 0, sizeof(msg));

    data.iov_base = &fd;
    data.iov_len = sizeof(fd);
    msg.msg_iov = &data;
    msg.msg_iovlen = 1;

    msg.msg_control = cmsgbuf;
    msg.msg_controllen = CMSG_LEN(sizeof(int));

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    *(int*)CMSG_DATA(cmsg) = fd;

    debug("sending fd %d", fd);

    if (0 >= sendmsg(socket, &msg, 0))
        err(1, "sendmsg(fd=%d)", fd);
}

static int
recv_fd(int socket, int* fdno)
{
    struct msghdr msg;
    int fd;
    struct iovec data;
    struct cmsghdr* cmsg;
    char cmsgbuf[CMSG_SPACE(sizeof(int))];

    memset(&msg, 0, sizeof(msg));

    data.iov_base = fdno;
    data.iov_len = sizeof(*fdno);
    msg.msg_iov = &data;
    msg.msg_iovlen = 1;

    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    if (0 >= recvmsg(parent_socket, &msg, 0))
        err(1, "recvmsg(fd)");

    cmsg = CMSG_FIRSTHDR(&msg);
    assert(cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type  == SCM_RIGHTS);
    fd = *(int*)CMSG_DATA(cmsg);

    debug("received fdno=%d as fd %d", *fdno, fd);

    return fd;
}

static void enable_counter(int fd)
{
    if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0))
        err(1, "ioctl(PERF_EVENT_ENABLE)");
}

static void disable_counter(int fd)
{
    if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0))
        err(1, "ioctl(PERF_EVENT_DISABLE)");
}

/*-----------------------------------------------------------------------------
 * Child
 */
static int counter_fd;

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
open_counter(size_t nr_switches)
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

static void arm_desched_notification()
{
    enable_counter(counter_fd);
}

static void disarm_desched_notification()
{
    disable_counter(counter_fd);
}

static void
chew_cpu(size_t ticks)
{
    size_t i;

    arm_desched_notification();

    for (i = 0; i < ticks; ++i) {
        if (0 == i % 10) {
            struct timeval tv;
            gettimeofday(&tv, NULL);
        } else {
            int j = i * (i / 31 + i / 77) % (i + 23);
            (void)j;
        }
    }

    disarm_desched_notification();
}

static void
spam_io(size_t ticks)
{
    int fd = open("/dev/null", O_WRONLY);
    ssize_t i;

    for (i = ticks - 1; i >= 0; --i) {
        arm_desched_notification();

#if 0
        sched_yield();
#elif 0
        /* This is very very unlikely to desched us. */
        //write(fd, i ? "." : "\n", 1);
        write(STDOUT_FILENO, i ? "." : "\n", 1);
#elif 0
        /* This has to desched us. */
        system("sleep 1");
#else
        struct timespec req = { .tv_sec = 1, .tv_nsec = 0 };
        struct timespec rem;
        nanosleep(&req, &rem);
#endif

        disarm_desched_notification();
    }

    close(fd);
}

static void
child()
{
    debug("child: execution info:\n"
           "  child() = %p\n"
           "  nr_switches = %u",
           child,
           nr_switches);
    if (IO == which) {
        debug("  spam_io() = %p, %u ticks", spam_io, io_ticks);
    } else {
        debug("  chew_cpu() = %p, %u ticks", chew_cpu, cpu_ticks);
    }

    counter_fd = open_counter(nr_switches);
    send_fd(counter_fd, child_socket);

    debug("child accepting ptrace");
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    raise(SIGSTOP);

    if (CPU == which) {
        chew_cpu(cpu_ticks);
    } else if (IO == which) {
        spam_io(io_ticks);
    } else {
        *((volatile int*)0) = 42;
    }

    _exit(0);
}

/*-----------------------------------------------------------------------------
 * Parent
 */
pid_t p;
int child_counter, child_counter_fdno;

static int
dump_status(pid_t p, int status)
{
    if (WIFSTOPPED(status)) {
        debug("%d stopped by signal %d", p, WSTOPSIG(status));
        return 1;
    } else if (WIFCONTINUED(status)) {
        debug("%d continued by SIGCONT", p);
        return 1;
    } else if (WIFEXITED(status)) {
        debug("%d exited with code %d", p, WEXITSTATUS(status));
        return 0;
    } else if (WIFSIGNALED(status)) {
        debug("%d terminated by signal %d", p, WTERMSIG(status));
        return 0;
    } else {
        debug("%d seems to be running normally", p);
        return 1;
    }
}

static void
dump_siginfo_regs(const char* pfx, pid_t tid, int status)
{
    struct siginfo si;
    struct user_regs_struct regs;

    ptrace(PTRACE_GETSIGINFO, tid, 0, &si);
    ptrace(PTRACE_GETREGS, tid, 0, &regs);
    debug("%s%d: stopped at %p (status 0x%x)\n"
          "    sig:%d (code:%d; fd:%d)\n"
          "    eax:%lx ebx:%lx ecx:%lx edx:%lx\n"
          "    esi:%lx edi:%lx\n"
          "    orig_eax:%ld\n"
          "    ebp:%lx esp:%lx\n"
          "    eip:%lx",
          pfx ? pfx : "", tid, (void*)regs.eip, status,
          si.si_signo, si.si_code, si.si_fd,
          regs.eax, regs.ebx, regs.ecx, regs.edx,
          regs.esi, regs.edi,
          regs.orig_eax,
          regs.ebp, regs.esp,
          regs.eip);
}

static void
parent()
{
    int nr_observed_descheds = 0;

    child_counter = recv_fd(parent_socket, &child_counter_fdno);

    while (1) {
        int status;

        waitpid(p, &status, 0);
        if (!dump_status(p, status)) {
            break;
        }

        if (ptrace(PTRACE_SETOPTIONS, p, 0, (void*)PTRACE_O_TRACESYSGOOD))
            err(1, "ptrace(SETOPTIONS)");

        if (WIFSTOPPED(status) && SIGIO == WSTOPSIG(status)) {
            uint64_t nr_desched;

            if (sizeof(nr_desched) != read(child_counter, &nr_desched,
                                           sizeof(nr_desched)))
                err(1, "read(child_counter)");
            disable_counter(child_counter);
            ++nr_observed_descheds;
            debug("  desched %llu; observed %d",
                  nr_desched, nr_observed_descheds);

            dump_siginfo_regs("  at first SIGIO: ", p, status);

            exit(0);

            debug("skip SIGIO:");
            ptrace(PTRACE_SYSCALL, p, 0, 0);
            waitpid(p, &status, 0);
            assert(WIFSTOPPED(status) && SIGIO == WSTOPSIG(status));
            dump_siginfo_regs("  after: ", p, status);

            debug("\nsyscall:");
            ptrace(PTRACE_SYSCALL, p, 0, 0);
            waitpid(p, &status, 0);
            dump_siginfo_regs("  after: ", p, status);

            debug("\nfinish syscall:");
            ptrace(PTRACE_SYSCALL, p, 0, 0);
            waitpid(p, &status, 0);
            dump_siginfo_regs("  after: ", p, status);
        }

        debug("%d continue", p);
        ptrace(PTRACE_CONT, p, 0, 0);
    }

    printf("child finished; observed %d descheds\n", nr_observed_descheds);
}

int
main(int argc, char** argv)
{
    int fds[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds))
        err(1, "socketpair()");

    parent_socket = fds[0];
    child_socket = fds[1];

    if (0 == (p = fork())) {
        child();
    } else {
        parent();
    }
    return 0;
}
