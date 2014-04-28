#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#define fatal(_msg, ...)                                                \
    do {                                                                \
        fprintf(stderr, "Fatal error: " _msg "\n", ## __VA_ARGS__);     \
        exit(1);                                                        \
    } while (0)

#define USER_WORD_DR(x) offsetof(struct user, u_debugreg[x])

typedef enum {
    TRAP_EXEC = 0x00, TRAP_WRITE = 0x01, TRAP_READWRITE = 0x03
} WatchType;
typedef enum {
    BYTES_1 = 0x00, BYTES_2 = 0x01, BYTES_4 = 0x03, BYTES_8 = 0x02
} WatchBytes;

struct DebugControl {
    uintptr_t dr0_local : 1;
    uintptr_t dr0_global : 1;
    uintptr_t dr1_local : 1;
    uintptr_t dr1_global : 1;
    uintptr_t dr2_local : 1;
    uintptr_t dr2_global : 1;
    uintptr_t dr3_local : 1;
    uintptr_t dr3_global : 1;

    uintptr_t ignored : 8;

    WatchType dr0_type : 2;
    WatchBytes dr0_len : 2;
    WatchType dr1_type : 2;
    WatchBytes dr1_len : 2;
    WatchType dr2_type : 2;
    WatchBytes dr2_len : 2;
    WatchType dr3_type : 2;
    WatchBytes dr3_len : 2;
};

static int mut = 0;

static int
read_mut(void)
{
    return mut;
}

static void
write_mut(int x)
{
    mut = x;
}

static void*
thread(void* unused)
{
    read_mut();
    write_mut(24);
    return NULL;
}

static void
child(void)
{
    pthread_t t;

    if (ptrace(PTRACE_TRACEME, 0, 0, 0)) {
        fatal("TRACEME in child");
    }
    kill(getpid(), SIGSTOP);

    read_mut();
    write_mut(42);

    pthread_create(&t, NULL, thread, NULL);
    pthread_join(t, NULL);

    exit(0);
}

static void
watch(pid_t tid, void* addr, WatchType what, WatchBytes len)
{
    struct DebugControl ctl = { 0 };
    if (ptrace(PTRACE_POKEUSER, tid, USER_WORD_DR(6), 0)) {
        fatal("Reset debug status");
    }
    if (ptrace(PTRACE_POKEUSER, tid, USER_WORD_DR(0), addr)) {
        fatal("Set debug reg 0");
    }
    ctl.dr0_local = 1;
    ctl.dr0_type = what;
    ctl.dr0_len = len;
    if (ptrace(PTRACE_POKEUSER, tid, USER_WORD_DR(7), ctl)) {
        fatal("Set debug ctl reg");
    }
}

int
main(void)
{
    void* read_mut_fn = read_mut;
    void* write_mut_fn = write_mut;
    int ret, status;
    struct user_regs_struct regs;
    void* ip;
    pid_t c = fork();
    pid_t t;
    if (0 == c) {
        child();
    }

    ret = waitpid(c, &status, 0);
    assert(c == ret && WIFSTOPPED(status) && SIGSTOP == WSTOPSIG(status));

    ptrace(PTRACE_SETOPTIONS, c, NULL,
           PTRACE_O_TRACECLONE | PTRACE_O_TRACESYSGOOD);

    watch(c, &mut, TRAP_WRITE, BYTES_4);
    ptrace(PTRACE_SYSCALL, c, NULL, NULL);

    ret = waitpid(-1, &status, __WALL);
    printf("task %d stopped with status %#x\n", ret, status);
    assert(c == ret && WIFSTOPPED(status) && SIGTRAP == WSTOPSIG(status));
    ptrace(PTRACE_GETREGS, c, 0, &regs);
    ip = (void*)regs.eip;
    printf(" -> hit write watchpoint at %p\n", ip);
    assert(read_mut_fn < write_mut_fn && write_mut_fn <= ip);

    ptrace(PTRACE_CONT, c, NULL, NULL);
    ret = waitpid(-1, &status, __WALL);
    printf("task %d stopped with status %#x\n", ret, status);
    assert(c == ret && 0x3057f == status/*PTRACE_EVENT_CLONE*/);
    ptrace(PTRACE_GETEVENTMSG, c, NULL, &t);

    ret = waitpid(t, &status, __WALL);
    printf("task %d stopped with status %#x\n", ret, status);

    watch(c, &mut, TRAP_READWRITE, BYTES_4);
    watch(t, &mut, TRAP_READWRITE, BYTES_4);
    ptrace(PTRACE_CONT, c, NULL, NULL);
    ptrace(PTRACE_CONT, t, NULL, NULL);

    ret = waitpid(-1, &status, __WALL);
    printf("task %d stopped with status %#x\n", ret, status);
    assert(t == ret && WIFSTOPPED(status) && SIGTRAP == WSTOPSIG(status));
    ptrace(PTRACE_GETREGS, t, 0, &regs);
    ip = (void*)regs.eip;
    printf(" -> hit read/write watchpoint at %p\n", ip);
    assert(read_mut_fn <= ip && read_mut_fn < write_mut_fn);

    return 0;
}
