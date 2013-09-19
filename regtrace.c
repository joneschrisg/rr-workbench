#define _GNU_SOURCE 1

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
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SYSCALL_PSEUDOSIG (0x80 | SIGTRAP)
#define PTRACE_EVENT(_status) ((0xFF0000 & (_status)) >> 16)

#define log(msg, ...)                           \
    fprintf(stderr, msg "\n", ## __VA_ARGS__)

#define fatal(msg, ...)                              \
    do {                                             \
        fprintf(stderr, msg "\n", ## __VA_ARGS__);   \
        abort();                                     \
    } while(0)

static const int use_filter = 0;

static int
trace(pid_t tid)
{
    enum { TRAPPING_SECCOMP, ENTERING_SYSCALL, EXITING_SYSCALL };
    int ret, status;
    const int start_state = use_filter ? TRAPPING_SECCOMP : ENTERING_SYSCALL;
    int state = start_state;

    ret = waitpid(tid, &status, 0);
    assert(tid == ret && WIFSTOPPED(status) && SIGSTOP == WSTOPSIG(status));

    if (ptrace(PTRACE_SETOPTIONS, tid, 0,
               (void*)(PTRACE_O_TRACESECCOMP | PTRACE_O_TRACESYSGOOD |
                       PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
                       PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC |
                       PTRACE_O_TRACEVFORKDONE | PTRACE_O_TRACEEXIT)))
        fatal("Failed to set ptrace options");

    while (1) {
        int event, sig;
        struct user_regs_struct regs;

        if (TRAPPING_SECCOMP == state) {
            log("  --> CONT");
            /* NB: This ignores all events up to the point just after
             * the filter is installed. */
            ptrace(PTRACE_CONT, tid, 0, 0);
        } else {
            log("  --> SYSCALL");
            ptrace(PTRACE_SYSCALL, tid, 0, 0);
        }
        waitpid(tid, &status, 0);

        event = PTRACE_EVENT(status);
        if (event) {
            log("  (ptrace event %d (status 0x%x))", event, status);
            state = (PTRACE_EVENT_EXEC == event) ? EXITING_SYSCALL :
                    ENTERING_SYSCALL;
            continue;
        }

        if (WIFEXITED(status)) {
            log("Exited normally with status %d", WEXITSTATUS(status));
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            log("Terminated by signal %d", WTERMSIG(status));
            return WTERMSIG(status);
        }

        assert(WIFSTOPPED(status));
        sig = WSTOPSIG(status);

        if (SYSCALL_PSEUDOSIG != sig && SIGTRAP != sig) {
            log("(stopped by signal %d)", sig);
            ptrace(PTRACE_SINGLESTEP, tid, 0, sig);
            continue;
        }

        ptrace(PTRACE_GETREGS, tid, 0, &regs);
        log("%d: %s syscall %ld (status 0x%x)\n"
            "     eax:%ld ebx:0x%lx ecx:0x%lx edx:0x%lx\n"
            "     esi:0x%lx edi:0x%lx ebp:0x%lx eip:%p",
            tid, ENTERING_SYSCALL == state ? "entering" : "exiting",
            regs.orig_eax, status,
            regs.eax, regs.ebx, regs.ecx, regs.edx,
            regs.esi, regs.edi, regs.ebp, (void*)regs.eip);

        if (SYSCALL_PSEUDOSIG == sig) {
            assert(ENTERING_SYSCALL == state || EXITING_SYSCALL == state);
            if (ENTERING_SYSCALL == state) {
                state = EXITING_SYSCALL;
            } else {
                state = start_state;
            }
        }
    }
}

int
main(int argc, char** argv)
{
    pid_t c = fork();

    if (0 == c) {
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        kill(getpid(), SIGSTOP);

        if (use_filter) {
            setenv("LD_PRELOAD", "libtraceseccomp.so", 1);
        }

        execvp(argv[1], argv + 1);
        fatal("Failed to exec tracee");
    }
    return trace(c);
}
