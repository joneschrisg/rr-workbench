#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <wait.h>

#define PTRACE_EVENT(_s) ((0xFF0000 & (_s)) >> 16)

static void
print_usage(const char* exe)
{
    fprintf(stderr, "Usage: %s <waitpid-status>...\n", exe);
}

int
main(int argc, char** argv)
{
    int i;

    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }

    for (i = 1; i < argc; ++i) {
        unsigned long status = strtoul(argv[i], NULL, 0);

        printf("0x%lx: ", status);
        if (WIFEXITED(status)) {
            printf("exited with code %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("terminated by signal %d%s\n",
                   WTERMSIG(status),
                   WCOREDUMP(status) ? " (dumped core)" : "");
        } else if (WIFSTOPPED(status)) {
            int ptrace_event = PTRACE_EVENT(status);
            printf("stopped by signal %d", WSTOPSIG(status));
            if (ptrace_event) {
                printf(" (ptrace event %d)", ptrace_event);
            }
            putc('\n', stdout);
        } else if (WIFCONTINUED(status)) {
            printf("resumed by SIGCONT\n");
        } else {
            printf("unknown status\n");
        }
    }

    return 0;
}
