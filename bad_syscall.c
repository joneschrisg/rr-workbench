#include <stdio.h>
#include <unistd.h>

int
main(int argc, char** argv)
{
    puts("entering syscall(-42) ...");
    syscall(-42, 1, 2, 3, 4, 5, 6);
    puts("done");
    return 0;
}
