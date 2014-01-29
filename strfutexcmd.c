#include <linux/futex.h>
#include <stdio.h>
#include <stdlib.h>

static const char*
strfutex(unsigned long cmd)
{
    switch (cmd) {
#define CASE(_c) case FUTEX_ ## _c: return #_c
    CASE(WAIT);
    CASE(WAKE);
    CASE(FD);
    CASE(REQUEUE);
    CASE(CMP_REQUEUE);
    CASE(WAKE_OP);
    CASE(LOCK_PI);
    CASE(UNLOCK_PI);
    CASE(TRYLOCK_PI);
    CASE(WAIT_BITSET);
    CASE(WAKE_BITSET);
    CASE(WAIT_REQUEUE_PI);
    CASE(CMP_REQUEUE_PI);
#undef CASE
    default:
        return "???";
    }
}

int
main(int argc, char** argv)
{
    int i;
    for (i = 1; i < argc; ++i) {
        unsigned long cmd = strtoul(argv[i], NULL, 0);
        int private = (cmd & FUTEX_PRIVATE_FLAG);
        int clock_realtime = (cmd & FUTEX_CLOCK_REALTIME);
        printf("0x%lx = %s%s%s\n",
            cmd,
            strfutex(cmd & FUTEX_CMD_MASK),
            private ? " (private)" : "",
            clock_realtime ? " (clock_realtime)" : "");
    }
    return 0;
}
