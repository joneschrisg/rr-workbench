#include <errno.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define fatal(msg, ...)                                 \
    do {                                                \
        fprintf(stderr, msg "\n", ##__VA_ARGS__);       \
        abort();                                        \
    } while (0)

typedef uint8_t byte;

static long (*vsyscall)(int syscall, ...) = (void*)0xb7fff414;
static const byte vsyscall_impl[] = {
    0x51,                       /* push %ecx */
    0x52,                       /* push %edx */
    0x55,                       /* push %ebp */
    0x89, 0xe5,                 /* mov %esp,%ebp */
    0x0f, 0x34,                 /* sysenter */
    0x90,                       /* nop */
    0x90,                       /* nop */
    0x90,                       /* nop */
    0x90,                       /* nop */
    0x90,                       /* nop */
    0x90,                       /* nop */
    0x90,                       /* nop */
    0xcd, 0x80,                 /* int $0x80 */
    0x5d,                       /* pop %ebp */
    0x5a,                       /* pop %edx */
    0x59,                       /* pop %ecx */
    0xc3,                       /* ret */
};

static const byte vsyscall_monkeypatch[] = {
    0x50,                         /* push %eax */
    0xb8, 0xc0, 0x86, 0x04, 0x08, /* mov $vsyscall_hook_trampoline, %eax */
    0xff, 0xe0,                   /* jmp *%eax */
};

__asm__(".text\n\t"
        "vsyscall_hook_trampoline:\n\t"
        "pop %eax\n\t"
        "push %ebp\n\t"
        "push %edi\n\t"
        "push %esi\n\t"
        "push %edx\n\t"
        "push %ecx\n\t"
        "push %ebx\n\t"
        "push %eax\n\t"
        "mov $vsyscall_hook, %eax\n\t"
        "call *%eax\n\t"
        "addl $4, %esp\n\t"
        "pop %ebx\n\t"
        "pop %ecx\n\t"
        "pop %edx\n\t"
        "pop %esi\n\t"
        "pop %edi\n\t"
        "pop %ebp\n\t"
        "ret\n\t"
    );

static int holy_crap_it_worked = 1;
static long last_syscallno;
static long last_a0;
static long last_ret;

static long
vsyscall_hook(long syscallno,
              long a0, long a1, long a2, long a3, long a4, long a5)
{
    long ret;
    __asm__ __volatile__("int $0x80"
                         : "=a"(ret)
                         : "0"(syscallno), "b"(a0), "c"(a1), "d"(a2),
                           "S"(a3), "D"(a4));

    holy_crap_it_worked = 1;
    last_syscallno = syscallno;
    last_a0 = a0;
    last_ret = ret;

    return ret;
}

static void
monkey_patch()
{
    byte* orig = (byte*)vsyscall;
    int i;
    for (i = 0; i < sizeof(vsyscall_monkeypatch); ++i) {
        orig[i] = vsyscall_monkeypatch[i];
    }
    printf("Successfully monkey-patched vdso to call %p!\n", vsyscall_hook);
}

static void
verify_vsyscall_insns(void* vsyscall)
{
    const byte* actual = vsyscall;
    int i;
    for (i = 0; i < sizeof(vsyscall_impl); ++i) {
        if (vsyscall_impl[i] != actual[i]) {
            fatal("Byte %d of __kernel_vsyscall should be 0x%x, but is 0x%x",
                  i, vsyscall_impl[i], actual[i]);
        }
    }
    printf("All insns match!\n");
}

int
main(int argc, char** argv)
{
    void* vdso_start = (void*)0xb7fff000;
    void* vdso_end = (void*)0xb8000000;
    size_t vdso_num_bytes = vdso_end - vdso_start;

    printf("vsyscall: %p\n", vsyscall);

    verify_vsyscall_insns(vsyscall);

    int ret = mprotect(vdso_start, vdso_num_bytes,
                       PROT_READ | PROT_WRITE | PROT_EXEC);
    int err = errno;
    printf("mprotect(%p, %d) -> %d (%s)\n", vdso_start, vdso_num_bytes,
           ret, strerror(err));

    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "cat /proc/%d/maps", getpid());
    system(cmd);

    monkey_patch();

    mprotect(vdso_start, vdso_num_bytes, PROT_READ | PROT_EXEC);

    if (holy_crap_it_worked) {
        printf("HOLY CRAP IT WORKED: %ld(0x%lx) -> %ld\n",
               last_syscallno, last_a0, last_ret);
    }

    return 0;
}

//0x80486d4 - 0x80486c0
