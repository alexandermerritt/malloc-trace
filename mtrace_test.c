#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "mtrace.h"

#define __unused    __attribute__((unused))

void* thread(void* __unused arg) {
    for (ul i = 0; i < (1ul<<16); i++) {
        volatile int *p1 = malloc(i+1);
        volatile int *p2 = calloc(i+1,1);
        *p2 = *p1; // prevent compiler optimizing them out
        free((void*)p1);
        free((void*)p2);
    }
    return NULL;
}

int main() {
    pthread_t tids[8];
    for (ul t = 0; t < 8; t++) {
        if (0 != pthread_create(&tids[t],
                    NULL, thread, NULL)) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    for (ul t = 0; t < 8; t++) {
        if (0 != pthread_join(tids[t], NULL)) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }
    return EXIT_SUCCESS;
}
