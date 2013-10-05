#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE_LEN  4096

#define fatal(msg, ...)                                                 \
    do {                                                                \
        fprintf(stderr, "Fatal error: %s (%d): "                        \
                msg "\n", strerror(errno), errno, ##__VA_ARGS__);       \
        exit(1);                                                        \
    } while (0)

static void
usage(const char* exe)
{
    fprintf(stderr, "Usage: %s <file1> <file2>\n", exe);
}

int
main(int argc, char** argv)
{
    const char* exe;
    const char* path1;
    const char* path2;
    FILE* f1;
    FILE* f2;

    if (argc != 3) {
        usage(argv[0]);
        exit(1);
    }
    exe = argv[0];
    path1 = argv[1];
    path2 = argv[2];

    if (!(f1 = fopen(path1, "r")))
        fatal("fopen(%s)", path1);
    if (!(f2 = fopen(path2, "r")))
        fatal("fopen(%s)", path2);

    printf("%s %s %s\n", exe, path1, path2);
    printf("--- %s\n", path1);
    printf("+++ %s\n", path2);

    while (!feof(f1) || !feof(f2)) {
        char buf1[MAX_LINE_LEN];
        const char* s1 = fgets(buf1, sizeof(buf1), f1);
        char buf2[MAX_LINE_LEN];
        const char* s2 = fgets(buf2, sizeof(buf2), f2);

        if (!s1 && !s2) {
            continue;
        }
        if (!s1) {
            printf("+%s", s2);
        } else if (!s2) {
            printf("-%s", s1);
        } else if (strcmp(s1, s2)) {
            printf("-%s", s1);
            printf("+%s", s2);
        }
    }

    fclose(f1);
    fclose(f2);
    return 0;
}
