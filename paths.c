#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static void paths(int nonce)
{
    if (nonce < 2)
        puts("path: nonce < 2");
    else
        puts("path: nonce >= 2");
}

static void* thread(void* pargc)
{
    int argc = (intptr_t)pargc;

    paths(argc);

    return NULL;
}

int main(int argc, char** argv)
{
    pthread_t t;

    pthread_create(&t, NULL, thread, (void*)(intptr_t)argc);

    paths(argc);

    pthread_join(t, NULL);

    return 0;
}
