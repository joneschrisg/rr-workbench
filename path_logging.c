/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#define DEBUGTAG "PathLogging"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* XXX split me out */
#define RR_MAGIC_SAVE_DATA_FD (-42)

#define PATH_RECORD_BUFFER_SIZE (1 << 10)

#ifdef DEBUGTAG
#  define DEBUG(msg, ...)						\
	fprintf(stderr, "[" DEBUGTAG "] " msg "\n", ## __VA_ARGS__)
#else
#  define DEBUG(msg, ...)
#endif

struct path_record {
	void* fn;
	int32_t path;
};

struct path_records {
	struct path_record recs[PATH_RECORD_BUFFER_SIZE];
	ssize_t len;
};

static __thread struct path_records record_buffer;

static void flush_buffer(struct path_records* buf)
{
	write(RR_MAGIC_SAVE_DATA_FD, &buf->recs,
	      buf->len * sizeof(buf->recs[0]));
	buf->len = 0;
}

void llvm_increment_path_count(uint32_t fn, uint32_t path)
{
	struct path_records* buf = &record_buffer;
	struct path_record* rec = &buf->recs[buf->len++];

	rec->fn = (void*)fn;
	rec->path = path;

	DEBUG("Retired (%p, %d)", rec->fn, rec->path);

	if (PATH_RECORD_BUFFER_SIZE == buf->len) {
		flush_buffer(buf);
	}
}

void llvm_decrement_path_count (uint32_t fn, uint32_t path)
{
	/* ignored */
}
