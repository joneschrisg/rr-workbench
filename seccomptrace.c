#include "seccomp-bpf.h"

#include <err.h>
#include <stdint.h>
#include <syscall.h>
#include <sys/prctl.h>
#include <unistd.h>

static void
ensure_filter()
{
    static int installed;
    if (!installed) {
        struct sock_filter filter[] = { TRACE_PROCESS };
	struct sock_fprog prog = {
		.len = (unsigned short)(sizeof(filter)/sizeof(filter[0])),
		.filter = filter,
	};
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
            err(1, "PR_SET_NO_NEW_PRIVS");
        if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, (uintptr_t)&prog))
            err(1, "SET_SECCOMP");

        syscall(SYS_write, 1, "filter on\n", sizeof("filter on\n") - 1);
        installed = 1;
    }
}

int puts(const char* s)
{
    ensure_filter();
    syscall(SYS_write, STDOUT_FILENO, s, strlen(s));
    syscall(SYS_write, STDOUT_FILENO, "\n", 1);
    return 1;
}
