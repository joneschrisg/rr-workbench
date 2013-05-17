/* -*- Mode: C++; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define fatal(what)  do { perror(what); abort(); } while (0)

struct process_stats {
	struct rusage usage;
        /* "The name can be up to 16 bytes long" */
	char name[32];
};

static void
get_process_name(struct process_stats* stats)
{
	char comm_path[PATH_MAX];
	int fd;
	ssize_t len;

	sprintf(comm_path, "/proc/%d/comm", getpid());
	fd = open(comm_path, O_RDONLY);
	if (fd < 0)
		fatal("open(comm_path)");

	len = read(fd, stats->name, sizeof(stats->name) - 1);
	if (len <= 1)
		fatal("read(comm_path)");

	/* eat newline character */
	stats->name[len - 1] = '\0';

#if 0
	/* XXX it'd be simpler to use prctl(PR_GET_NAME) here, but for
	 * some reason under rr that's "" for xpcshell.  without rr,
	 * it's "xpcshell", as expected. */
	if (prctl(PR_GET_NAME, stats->name))
		fatal("prctl(PR_GET_NAME)");
#endif
}

static void
get_process_stats(struct process_stats* stats)
{
	if (getrusage(RUSAGE_SELF, &stats->usage))
		fatal("getrusage(RUSAGE_SELF)");
	get_process_name(stats);
}

static void
dump_process_stats(void)
{
	struct process_stats stats;
	memset(&stats, 0, sizeof(stats));
	get_process_stats(&stats);

	printf("RRMON[%d][%s]: vol: %ld; nonvol: %ld; sig: %ld"
	       "; hard-fault: %ld\n",
	       getpid(), stats.name,
	       stats.usage.ru_nvcsw, stats.usage.ru_nivcsw,
	       stats.usage.ru_nsignals,
	       stats.usage.ru_majflt);
}

static void
sighandler(int signo)
{
	/* XXX assuming fatal for now */
	/* XXX this is unsafe in general */
	dump_process_stats();
}

static void init() __attribute__((constructor));
static void
init()
{
	int signals[] = { SIGABRT, SIGSEGV };
	int i;
	for (i = 0; i < sizeof(signals)/sizeof(signals[0]); ++i)
		signal(signals[i], sighandler);
}

static void deinit() __attribute__((destructor));
static void
deinit()
{
	dump_process_stats();
}
