#include <stdlib.h>
#include "mtrace.h"
int main() {
    for (ul i = 0; i < 10000; i++)
        free(malloc(i+1));
    for (ul i = 0; i < 10000; i++)
        free(calloc(i+1,1));
    return EXIT_SUCCESS;
}
