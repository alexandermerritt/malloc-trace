#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "mtrace.h"

#define NENTRIES    1024

void unpack(char *fname) {
    FILE *fp = fopen(fname, "r");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    struct entry *entries =
        calloc(NENTRIES, sizeof(*entries));
    assert(entries);

    size_t ne;
    while ((ne = fread(entries, sizeof(*entries), NENTRIES, fp))) {
        for (ul i = 0; i < ne; i++) {
            struct entry *e = &entries[i];
            switch (e->op) {
                case OP_MALLOC: {
                    printf("%lu m %lu %p\n",
                            e->tsc, e->args.ma.size,
                            e->args.ma.ptr);
                } break;
                case OP_FREE: {
                    printf("%lu f %p\n",
                            e->tsc, e->args.fr.ptr);
                } break;
                case OP_CALLOC: {
                    printf("%lu c %lu %lu %p\n",
                            e->tsc, e->args.ca.nmemb,
                            e->args.ca.size, e->args.ca.ret);
                } break;
                case OP_REALLOC: {
                    printf("%lu r %p %lu %p\n",
                            e->tsc, e->args.re.ptr,
                            e->args.re.size, e->args.re.ret);
                } break;
                default: abort();
            }
        }
        clearerr(fp);
    }
    if (ne == 0) {
        if (ferror(fp)) {
            perror("fread");
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);
}

int main() {
    unpack("mtrace.log");
    return EXIT_SUCCESS;
}
