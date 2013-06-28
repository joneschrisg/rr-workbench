#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#define SYS_rpc -42

static int
child()
{
    int ret;
    const char str[] = "hi\n";
    size_t len = sizeof(str) - 1;

    printf("child: will request write of str=%p, len=%d\n", str, len);

    ptrace(PTRACE_TRACEME, 0, 0, 0);
    raise(SIGSTOP);

    /* rpc(SYS_write, str, len); */
    __asm__ __volatile__("int $0x80\n\t"
                         : "=a"(ret)
                         : "0"(SYS_rpc), "b"(SYS_write), "c"(str), "d"(len));
    /* (allocate dummy syscall for parent; not needed "in production",
     * since parent can inject this itself) */
    __asm__ __volatile__("int $0x80");

    printf("child got %d from rpc(write)\n", ret);

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
    if (SYS_rpc == regs.orig_eax) {
        int syscallno = regs.ebx;
        void* str = (void*)regs.ecx;
        int len = regs.edx;
        struct user_regs_struct callregs;
        int j;

        printf("parent: child requests rpc for syscall %d (write; str=%p, len=%d)\n",
               syscallno, str, len);

        puts("parent: advance to rpc exit");
        ptrace(PTRACE_SYSCALL, c, 0, 0);
        waitpid(c, &status, 0);
        dump_status(c, status);

        /* registers for return to rpc syscall */
        ptrace(PTRACE_GETREGS, c, 0, &regs);
        regs.eax = 0;
        printf("parent: setting ret=0 for ip %p\n", (void*)regs.eip);

        /* (in production, overwrite eip with int 0x80) */
        memcpy(&callregs, &regs, sizeof(callregs));
        for (j = 0; j < 5; ++j) {
            /* set up artificial syscall */
            callregs.eax = SYS_write;
            callregs.ebx = STDOUT_FILENO;
            callregs.ecx = (uintptr_t)str;
            callregs.edx = len;
            ptrace(PTRACE_SETREGS, c, 0, &callregs);
            callregs.eip = regs.eip;

            puts("parent: entering artificial syscall ...");
            ptrace(PTRACE_SYSCALL, c, 0, 0);
            waitpid(c, &status, 0);
            dump_status(c, status);

            puts("parent: ... finishing syscall ...");
            ptrace(PTRACE_SYSCALL, c, 0, 0);
            waitpid(c, &status, 0);
            dump_status(c, status);
        }

        puts("parent: done, restoring regs from before phony syscall");
        /* restore eip */
        ptrace(PTRACE_SETREGS, c, 0, &regs);

        /* (enter/finish stub syscall that we wouldn't hit in
         * production) */
        ptrace(PTRACE_SYSCALL, c, 0, 0);
        waitpid(c, &status, 0);
        ptrace(PTRACE_SYSCALL, c, 0, 0);
        waitpid(c, &status, 0);
    }

    do {
        ptrace(PTRACE_SYSCALL, c, 0, 0);
        waitpid(c, &status, 0);

        ptrace(PTRACE_GETREGS, c, 0, &regs);
        printf("parent: child at/in syscall %ld\n", regs.orig_eax);
    } while (dump_status(c, status));

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
