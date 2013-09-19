#include <stdio.h>
#include <unistd.h>

int
main(int argc, char** argv)
{
    const int bad_syscall = 500;

    puts("entering bad syscall ...");
    syscall(bad_syscall, 1, 2, 3, 4, 5, 6);
    puts("done");

    return 0;
}
