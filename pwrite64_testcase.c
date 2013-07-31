/* -*- Mode: C; tab-width: 8; c-basic-offset: 8; indent-tabs-mode: t; -*- */

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define test_assert(cond)  assert("FAILED if not: " && (cond))

#define TEST_FILE "prw.txt"

int main(int argc, char *argv[]) {
	int fd = open(TEST_FILE, O_CREAT | O_RDWR, 0600);
	const char content[] = "01234567890\nhello there\n";
	char buf[sizeof(content)];
	ssize_t nr;

	memset(buf, '?', sizeof(buf));
	nr = write(fd, buf, sizeof(buf));
	test_assert(nr == sizeof(buf));
	nr = write(fd, buf, 10);
	test_assert(nr == 10);

#if 0
	nr = pwrite(fd, content, sizeof(content), 10);
	nr = syscall(SYS_pwrite64, fd, content, sizeof(content), 10, 0);
#endif
	__asm__ __volatile__("int $0x80"
			     : "=a"(nr)
			     : "0"(SYS_pwrite64), "b"(fd), "c"(content),
			       "d"(sizeof(content)), "S"(10), "D"(0));

	test_assert(nr == sizeof(content));
	printf("Wrote ```%s'''\n", content);

	nr = pread(fd, buf, sizeof(buf), 10);
	test_assert(nr == sizeof(content));
	printf("Read ```%s'''\n", buf);

	close(fd);
	unlink(TEST_FILE);

	puts("EXIT-SUCCESS");
	return 0;
}
