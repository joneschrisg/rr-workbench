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

#define SYSCALL_PSEUDOSIG (0x80 | SIGTRAP)

#define fatal(msg, ...)                         \
    do {                                        \
        fprintf(stderr, msg, ## __VA_ARGS__);   \
        abort();                                \
    } while(0)

static int
trace(pid_t tid)
{
    waitpid(tid, NULL, 0);
    if (ptrace(PTRACE_SETOPTIONS, tid, 0, (void*)PTRACE_O_TRACESYSGOOD))
        fatal("Failed to set TRACESYSGOOD");

    while (1) {
        int status, sig;
        struct user_regs_struct regs;

        ptrace(PTRACE_SYSCALL, tid, 0, 0);
        waitpid(tid, &status, 0);

        if (WIFEXITED(status)) {
            printf("Exited normally with status %d\n", WEXITSTATUS(status));
            return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
            printf("Terminated by signal %d\n", WTERMSIG(status));
            return WTERMSIG(status);
        }

        assert(WIFSTOPPED(status));
        sig = WSTOPSIG(status);
        if (SYSCALL_PSEUDOSIG != sig && SIGTRAP != sig) {
            printf("(stopped by signal %d)\n", sig);
            ptrace(PTRACE_SINGLESTEP, tid, 0, sig);
            continue;
        }

        ptrace(PTRACE_GETREGS, tid, 0, &regs);
        if (SIGTRAP == sig && SYS_execve == regs.orig_eax) {
            printf("(exec'd)\n");
            continue;
        }

        printf("%ld: eax:0x%lx ebx:0x%lx ecx:0x%lx edx:0x%lx\n"
               "     esi:0x%lx edi:0x%lx ebp:0x%lx eip:%p\n",
               regs.orig_eax, regs.eax, regs.ebx, regs.ecx, regs.edx,
               regs.esi, regs.edi, regs.ebp, (void*)regs.eip);
    }
}

int
main(int argc, char** argv)
{
    pid_t c = fork();
    if (0 == c) {
        ptrace(PTRACE_TRACEME, 0, 0, 0);
        raise(SIGSTOP);

        execvp(argv[1], argv + 1);
        fatal("Failed to exec tracee");
    }
    return trace(c);
}
