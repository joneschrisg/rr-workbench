/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

//#define DEBUGTAG "PathLogging"

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <rr/rr.h>

#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(x) ((x + PAGE_SIZE - 1) & PAGE_MASK)

#define PATH_RECORD_BUFFER_SIZE (1 << 15)

#ifdef DEBUGTAG
#  define DEBUG(msg, ...)						\
	fprintf(stderr, "[" DEBUGTAG "][%ld] " msg "\n",		\
		syscall(SYS_gettid), ## __VA_ARGS__)
#else
#  define DEBUG(msg, ...)
#endif

struct path_record {
	void* fn;
	int32_t path;
};

struct path_records {
	struct path_record recs[PATH_RECORD_BUFFER_SIZE];
	ssize_t len : 30;
	ssize_t finished : 1;
};

static pthread_once_t init_process_once = PTHREAD_ONCE_INIT;
static pthread_key_t finish_buffer_key;

static __thread struct path_records* record_buffer;

static int sys_write(int fd, void* buf, size_t len)
{
	return syscall(SYS_write, fd, buf, len);
}

static void flush_buffer(struct path_records* buf)
{
	DEBUG("Flushing buffer");

	sys_write(RR_MAGIC_SAVE_DATA_FD, &buf->recs,
		  buf->len * sizeof(buf->recs[0]));
	buf->len = 0;
}

static void drop_buffer(void)
{
	struct path_records* buf = record_buffer;

	pthread_setspecific(finish_buffer_key, NULL);
	record_buffer = NULL;
	munmap(buf, PAGE_ALIGN(sizeof(*buf)));
}

static void finish_buffer(void* pbuf)
{
	struct path_records* buf = pbuf;

	DEBUG("Finishing buffer");
	flush_buffer(buf);
	drop_buffer();
}

static void finish_main_thread_buffer_hack(void)
{
	struct path_records* buf = record_buffer;
	if (buf) {
		finish_buffer(buf);
	}
}

static void init_process(void)
{
	pthread_atfork(NULL, NULL, drop_buffer);
	pthread_key_create(&finish_buffer_key, finish_buffer);
	atexit(finish_main_thread_buffer_hack);
}

static struct path_records* ensure_thread_init(void)
{
	struct path_records* buf = record_buffer;

	if (buf) {
		return buf;
	}

	DEBUG("Initializing buffer");
	pthread_once(&init_process_once, init_process);

	buf = mmap(NULL, PAGE_ALIGN(sizeof(*buf)),
		   PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	record_buffer = buf;
	pthread_setspecific(finish_buffer_key, buf);

	return buf;
}

void llvm_increment_path_count(int8_t* fn, int32_t path)
{
	struct path_records* buf = ensure_thread_init();
	struct path_record* rec = &buf->recs[buf->len++];

	rec->fn = fn;
	rec->path = path;

	DEBUG("Retired (%p, %d)", rec->fn, rec->path);

	if (PATH_RECORD_BUFFER_SIZE == buf->len) {
		flush_buffer(buf);
	}
}

void llvm_decrement_path_count (int8_t* fn, int32_t path)
{
	/* ignored */
}
