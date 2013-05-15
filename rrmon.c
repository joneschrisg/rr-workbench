/* -*- Mode: C++; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

struct process_stats {
	uint64_t voluntary_ctxt_switches;
	uint64_t nonvoluntary_ctxt_switches;	
};

int read_process_stats(pid_t pid, struct process_stats* stats) {
	char status_path[PATH_MAX];
	FILE* status_file;
	char* line = NULL;
	size_t line_len = 0;

	snprintf(status_path, sizeof(status_path) - 1, "/proc/%d/status", pid);
	status_file = fopen(status_path, "r");
	if (!status_file) {
		return -1;
	}

	while(-1 != getline(&line, &line_len, status_file)) {
		int nmatched;
		nmatched = sscanf(line, "voluntary_ctxt_switches: %" PRIu64,
				  &stats->voluntary_ctxt_switches);
		if (nmatched == 1) {
			continue;
		}
		nmatched = sscanf(line, "nonvoluntary_ctxt_switches: %" PRIu64,
				  &stats->nonvoluntary_ctxt_switches);
	}

	fclose(status_file);
	free(line);

	return 0;
}

static void deinit() __attribute__((destructor));
static void deinit()
{
	struct process_stats stats;
	memset(&stats, 0, sizeof(stats));

	if (read_process_stats(getpid(), &stats)) {
		fprintf(stderr, "Failed to get process stats");
		return;
	}

	printf("RRMON[%d] vol: %" PRIu64 "; nonvol: %" PRIu64 "\n",
	       getpid(),
	       stats.voluntary_ctxt_switches,
	       stats.nonvoluntary_ctxt_switches);
}
