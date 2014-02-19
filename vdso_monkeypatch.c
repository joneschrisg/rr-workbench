#include <assert.h>
#include <errno.h>
#include <linux/limits.h>
#include <stddef.h>
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

/* x86 native kernel. */
#if 0
static void* vdso_start_addr = (void*)0xb7fff000;
static const size_t vdso_num_bytes = 4096;
static const size_t vsyscall_offset = 0x414;
#else
/* x86 process on x64 kernel. */
static void* vdso_start_addr = (void*)0xf7ffb000;
static const size_t vdso_num_bytes = 4096;
static const size_t vsyscall_offset = 0x420;
#endif

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

/* NBB: the placeholder bytes in |struct insns_template| below must be
 * kept in sync with this. */
static const byte vsyscall_monkeypatch[] = {
    0x50,                         /* push %eax */
    0xb8, 0x00, 0x00, 0x00, 0x00, /* mov $_vsyscall_hook_trampoline, %eax */
    /* The immediate param of the |mov| is filled in dynamically by
     * the template mechanism below.  The NULL here is a
     * placeholder. */
    0xff, 0xe0,                   /* jmp *%eax */
};

struct insns_template {
    /* NBB: |vsyscall_monkeypatch| must be kept in sync with these
     * placeholder bytes. */
    byte push_eax_insn;
    byte mov_vsyscall_hook_trampoline_eax_insn;
    void* vsyscall_hook_trampoline_addr;
} __attribute__((packed));

typedef union {
    byte bytes[sizeof(vsyscall_monkeypatch)];
    struct insns_template insns;
} __attribute__((packed)) monkeypatch_template;

/**
 * Save the syscall params to this struct so that we can pass around a
 * pointer to it and avoid pushing/popping all this data for calls to
 * subsequent handlers.
 */
struct syscall_info {
    long no;
    long args[6];
};

__asm__(".text\n\t"
        ".globl _vsyscall_hook_trampoline\n\t"
        ".type _vsyscall_hook_trampoline, @function\n\t"
        "_vsyscall_hook_trampoline:\n\t"
	".cfi_startproc\n\t"

        /* The monkeypatch pushed $eax on the stack, but there's no
         * CFI info for it.  Fix up the CFA offset here to account for
         * the monkeypatch code. */
        ".cfi_adjust_cfa_offset 4\n\t"
        ".cfi_rel_offset %eax, 0\n\t"

        /* Pull $eax back off the stack.  Now our syscall-arg
         * registers are restored to their state on entry to
         * __kernel_vsyscall(). */
        "popl %eax\n\t"
	".cfi_adjust_cfa_offset -4\n\t"
	".cfi_restore %eax\n\t"

        /* Build a |struct syscall_info| by pushing all the syscall
         * args and the number onto the stack. */
                                /* struct syscall_info info; */
        "pushl %ebp\n\t"        /* info.args[5] = $ebp; */
	".cfi_adjust_cfa_offset 4\n\t"
	".cfi_rel_offset %ebp, 0\n\t"
        "pushl %edi\n\t"        /* info.args[4] = $edi; */
	".cfi_adjust_cfa_offset 4\n\t"
	".cfi_rel_offset %edi, 0\n\t"
        "pushl %esi\n\t"        /* info.args[3] = $esi; */
	".cfi_adjust_cfa_offset 4\n\t"
	".cfi_rel_offset %esi, 0\n\t"
        "pushl %edx\n\t"        /* info.args[2] = $edx; */
	".cfi_adjust_cfa_offset 4\n\t"
	".cfi_rel_offset %edx, 0\n\t"
        "pushl %ecx\n\t"        /* info.args[1] = $ecx; */
	".cfi_adjust_cfa_offset 4\n\t"
	".cfi_rel_offset %ecx, 0\n\t"
        "pushl %ebx\n\t"        /* info.args[0] = $ebx; */
	".cfi_adjust_cfa_offset 4\n\t"
	".cfi_rel_offset %ebx, 0\n\t"
        "pushl %eax\n\t"        /* info.no = $eax; */
	".cfi_adjust_cfa_offset 4\n\t"

        /* $esp points at &info.  Push that pointer on the stack as
         * our arg for syscall_hook(). */
        "movl %esp, %ecx\n\t"
        "pushl %ecx\n\t"
	".cfi_adjust_cfa_offset 4\n\t"

        "movl $vsyscall_hook, %eax\n\t"
        "call *%eax\n\t"        /* $eax = vsyscall_hook(&info); */

        /* $eax is now the syscall return value.  Erase the |&info|
         * arg and |info.no| from the stack so that we can restore the
         * other registers we saved. */
        "addl $8, %esp\n\t"
	".cfi_adjust_cfa_offset -8\n\t"

        /* Contract of __kernel_vsyscall() is that even callee-save
         * registers aren't touched, so we restore everything here. */
        "popl %ebx\n\t"
	".cfi_adjust_cfa_offset -4\n\t"
	".cfi_restore %ebx\n\t"
        "popl %ecx\n\t"
	".cfi_adjust_cfa_offset -4\n\t"
	".cfi_restore %ecx\n\t"
        "popl %edx\n\t"
	".cfi_adjust_cfa_offset -4\n\t"
	".cfi_restore %edx\n\t"
        "popl %esi\n\t"
	".cfi_adjust_cfa_offset -4\n\t"
	".cfi_restore %esi\n\t"
        "popl %edi\n\t"
	".cfi_adjust_cfa_offset -4\n\t"
	".cfi_restore %edi\n\t"
        "popl %ebp\n\t"
	".cfi_adjust_cfa_offset -4\n\t"
	".cfi_restore %ebp\n\t"

        /* Return to the caller of *|__kernel_vsyscall()|*, because
         * the monkeypatch jumped to us. */
        "ret\n\t"
    	".cfi_endproc\n\t"
        ".size _vsyscall_hook_trampoline, .-_vsyscall_hook_trampoline\n\t");

static void* get_vsyscall_hook_trampoline(void)
{
    void *ret;
    __asm__ __volatile__(
	    "call .L_get_vsyscall_hook_trampoline__pic_helper\n\t"
	    ".L_get_vsyscall_hook_trampoline__pic_helper: pop %0\n\t"
	    "addl $(_vsyscall_hook_trampoline - .L_get_vsyscall_hook_trampoline__pic_helper),%0"
	    : "=a"(ret));
    return ret;
}

static long
vsyscall_hook(const struct syscall_info* call)
{
    long ret;
    __asm__ __volatile__("int $0x80"
                         : "=a"(ret)
                         : "0"(call->no), "b"(call->args[0]),
                           "c"(call->args[1]), "d"(call->args[2]),
                           "S"(call->args[3]), "D"(call->args[4]));
    /* *(volatile int*)0 = 42; */
    return ret;
}

static void*
__kernel_vsyscall_addr()
{
    return vdso_start_addr + vsyscall_offset;
}

static void
monkey_patch(void* vsyscall)
{
    monkeypatch_template t;
    byte* orig = vsyscall;
    int i;

    memcpy(t.bytes, vsyscall_monkeypatch, sizeof(t.bytes));
    /* Try to catch out-of-sync |vsyscall_monkeypatch| and |struct
     * insns_template|. */
    assert(NULL == t.insns.vsyscall_hook_trampoline_addr);
    t.insns.vsyscall_hook_trampoline_addr = get_vsyscall_hook_trampoline();

    for (i = 0; i < sizeof(t.bytes); ++i) {
        orig[i] = t.bytes[i];
    }
    /* If the monkeypatch works, this write() call will be routed
     * through our vdso vsyscall hook. */
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
    void* vsyscall = __kernel_vsyscall_addr();
    printf("vsyscall: %p\n", vsyscall);

    verify_vsyscall_insns(vsyscall);

    int ret = mprotect(vdso_start_addr, vdso_num_bytes,
                       PROT_READ | PROT_WRITE | PROT_EXEC);
    int err = errno;
    printf("mprotect(%p, %d) -> %d (%s)\n", vdso_start_addr, vdso_num_bytes,
           ret, strerror(err));

    char cmd[PATH_MAX];
    snprintf(cmd, sizeof(cmd), "cat /proc/%d/maps", getpid());
    system(cmd);

    monkey_patch(vsyscall);

    mprotect(vdso_start_addr, vdso_num_bytes, PROT_READ | PROT_EXEC);

    return 0;
}
