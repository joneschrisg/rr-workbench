/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#define DEBUGTAG "PathLogging"

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
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

static pthread_once_t init_process_once = PTHREAD_ONCE_INIT;
static pthread_key_t finish_buffer_key;

static __thread struct path_records* record_buffer;

static pid_t sys_gettid(void)
{
	return syscall(SYS_gettid);
}

static void flush_buffer(struct path_records* buf)
{
	write(RR_MAGIC_SAVE_DATA_FD, &buf->recs,
	      buf->len * sizeof(buf->recs[0]));
	buf->len = 0;
}

static void finish_buffer(void* pbuf)
{
	struct path_records* buf = pbuf;

	assert(buf == record_buffer);
	DEBUG("Finishing task %d", sys_gettid());

	flush_buffer(buf);
	free(buf);
}

static void drop_buffer(void)
{
	record_buffer = NULL;
	pthread_setspecific(finish_buffer_key, NULL);
}

static void init_process(void)
{
	pthread_atfork(NULL, NULL, drop_buffer);
	pthread_key_create(&finish_buffer_key, finish_buffer);
}

static struct path_records* ensure_thread_init(void)
{
	struct path_records* buf;

	if (record_buffer) {
		return record_buffer;
	}

	DEBUG("Initializing task %d", sys_gettid());

	pthread_once(&init_process_once, init_process);

	buf = calloc(1, sizeof(*buf));
	pthread_setspecific(finish_buffer_key, buf);
	record_buffer = buf;

	assert(pthread_getspecific(finish_buffer_key) == record_buffer);

	return record_buffer;
}

void llvm_increment_path_count(uint32_t fn, uint32_t path)
{
	struct path_records* buf = ensure_thread_init();
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
