#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define fatal(msg, ...)                                         \
    do {                                                        \
        fprintf(stderr, "Fatal: %s(%d)" msg "\n",               \
                strerror(errno), errno, ## __VA_ARGS__);        \
        _exit(1);                                               \
    } while (0)

int
main(int argc, char** argv)
{
    struct timeval tv;
    pid_t pid;
    int status;

    gettimeofday(&tv, NULL);
    printf("tod: %ld\n", tv.tv_sec);

    if (argc < 2) {
        fatal("Usage: %s <exe-image> <args...>", argv[0]);
    }

    pid = fork();
    if (0 == pid) {
        execv(argv[1], &argv[1]);
        fatal("exec() failed");
    }

    waitpid(pid, &status, 0);
    printf("Child exited with status 0x%x\n", status);
    return 0;
}
