#include <stdio.h>
#include <syscall.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

static int
child()
{
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    raise(SIGSTOP);

    int ret = syscall(-42);
    printf("child got %d from syscall\n", ret);

    return 0;
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

static int
parent(pid_t c)
{
    int status;
    struct user_regs_struct regs;

    /* skip past signal */
    ptrace(PTRACE_CONT, c, 0, 0);
    waitpid(c, &status, 0);

    ptrace(PTRACE_SYSCALL, c, 0, 0);
    waitpid(c, &status, 0);
    dump_status(c, status);
    ptrace(PTRACE_GETREGS, c, 0, &regs);
    printf("child entering syscall: %ld\n", regs.orig_eax);

    ptrace(PTRACE_SYSCALL, c, 0, 0);
    waitpid(c, &status, 0);
    dump_status(c, status);
    ptrace(PTRACE_GETREGS, c, 0, &regs);
    printf("child exiting syscall with ret: %ld\n", regs.eax);
    regs.eax = 42;
    ptrace(PTRACE_SETREGS, c, 0, &regs);

    ptrace(PTRACE_CONT, c, 0, 0);
    waitpid(c, &status, 0);

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
