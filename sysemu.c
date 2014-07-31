#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <sys/ptrace.h>
#include <asm/ptrace-abi.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int
child()
{
    struct timeval tv;
    struct timespec ts;

    ptrace(PTRACE_TRACEME, 0, 0, 0);
    raise(SIGSTOP);

    gettimeofday(&tv, NULL);
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return 0;
}

static int
dump_status(pid_t p, int status)
{
    if (WIFSTOPPED(status)) {
        char cmd[PATH_MAX];
        printf("%d stopped by signal %d\n", p, WSTOPSIG(status));
        snprintf(cmd, sizeof(cmd), "cat /proc/%d/syscall", p);
        system(cmd);
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

static int
parent(pid_t c)
{
    int status;
    struct user_regs_struct regs;

    waitpid(c, &status, 0);
    dump_status(c, status);
    ptrace(PTRACE_GETREGS, c, 0, &regs);
    printf("  orig_eax: %ld\n", regs.orig_eax);

    ptrace(PTRACE_SYSEMU, c, 0, 0);
    waitpid(c, &status, 0);
    dump_status(c, status);
    ptrace(PTRACE_GETREGS, c, 0, &regs);
    printf("  orig_eax: %ld\n", regs.orig_eax);

    ptrace(PTRACE_SYSEMU, c, 0, 0);
    waitpid(c, &status, 0);
    dump_status(c, status);
    ptrace(PTRACE_GETREGS, c, 0, &regs);
    printf("  ip:%#lx; orig_eax:%ld\n", regs.eip, regs.orig_eax);
    /* finish ? ->*/
    ptrace(PTRACE_SYSEMU_SINGLESTEP, c, 0, 0);
    waitpid(c, &status, 0);
    dump_status(c, status);
    ptrace(PTRACE_GETREGS, c, 0, &regs);
    printf("  ip:%#lx; orig_eax:%ld\n", regs.eip, regs.orig_eax);

    ptrace(PTRACE_SYSEMU, c, 0, 0);
    waitpid(c, &status, 0);
    dump_status(c, status);
    ptrace(PTRACE_GETREGS, c, 0, &regs);
    printf("  orig_eax: %ld\n", regs.orig_eax);

    return 0;
}

int
main(int argc, char** argv)
{
    pid_t c;

    if (0 == (c = fork())) {
        return child();
    }
    return parent(c);
}
