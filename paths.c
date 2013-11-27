#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define NUM_ITS (1 << 20)
#define NUM_THREADS 8

static FILE* f;

static void paths(int nonce)
{
    if (nonce < 2)
        fputs("path: nonce < 2\n", f);
    else
        fputs("path: nonce >= 2\n", f);
}

static void* thread(void* pargc)
{
    int argc = (intptr_t)pargc;
    int i;

    for (i = 0; i < NUM_ITS; ++i) {
        paths(argc);
    }

    return NULL;
}

int main(int argc, char** argv)
{
    pthread_t t[NUM_THREADS];
    int i;

    f = fopen("/dev/null", "w");

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_create(&t[i], NULL, thread, (void*)(intptr_t)argc);
    }

    for (i = 0; i < NUM_ITS; ++i) {
        paths(argc);
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(t[i], NULL);
    }

    return 0;
}
