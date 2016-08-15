#include <sys/mman.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <map>

using namespace std;

typedef unsigned long ul;
typedef uint64_t u64;

struct entry {
    long key, addr, size;
};

#define ENTRIES_PER_BUCKET  8

struct bucket {
    struct entry entries[ENTRIES_PER_BUCKET];
};

static struct bucket *table;
static ul nbuckets; // should be power of 2
static ul bucket_len; // should be power of 2

struct mem {
    unsigned long vsize;
    long rss;
};

static unsigned long cur_alloc = 0ul;

static char path[] = "/proc/self/stat";

#define FNV_OFFSET_BASIS_64     14695981039346656037ul
#define FNV_PRIME_64            1099511628211ul

static inline
u64 fnv(u64 val) {
    u64 hash = FNV_OFFSET_BASIS_64;
    uint8_t *p = (uint8_t*)&val;
    for (ul i = 0; i < 8; i++)
        hash = (hash ^ p[i]) * FNV_PRIME_64;
    return hash;
}

static inline
ul make_hash(long value) {
    return fnv(value);
}

// prevent compiler removing calls to memset
__attribute__((noinline))
static
void touch(void *where, size_t len) {
    memset(where, 0xab, len);
}

// we subtract out the size of the table
struct mem getmem() {
    static char *buf = NULL;
    if (!buf)
        assert((buf = (char*)calloc(1, 1024)));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    fgets(buf, 1024, fp);
    fclose(fp);
    fp = NULL;
    int field_idx = 22; // max 51
    char *pos = buf;
    while (field_idx-- > 0)
        pos = &strchrnul(pos, ' ')[1];
    struct mem mem;
    mem.vsize = strtol(pos, NULL, 10);// - bucket_len;
    // next field
    pos = &strchrnul(pos, ' ')[1];
    mem.rss = 4096*strtol(pos, NULL, 10);// - bucket_len;
    return mem;
}

static const ul print_every = 100ul * 1000ul;

void print_mem() {
    struct mem mem = getmem();
    printf("%11lu (%6lu MiB) %11ld (%6lu MiB) | %11lu (%6lu MiB)"
            " | %4.2lf %4.2lf\n",
            mem.vsize, (ul)((float)mem.vsize/(1ul<<20)),
            mem.rss, (ul)((float)mem.rss/(1ul<<20)),
            cur_alloc, (ul)((float)cur_alloc/(1ul<<20)),
            (float)mem.vsize/(float)cur_alloc,
            (float)mem.rss/(float)cur_alloc);
}

void init_table(long n) {
    assert(__builtin_popcount(n) == 1);
    nbuckets = 2 * n / ENTRIES_PER_BUCKET;
    assert(__builtin_popcount(nbuckets) == 1);
    size_t len = nbuckets*ENTRIES_PER_BUCKET*sizeof(*table);
    table = (struct bucket*)mmap(NULL, len,
            PROT_READ|PROT_WRITE,
            MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    assert(table != MAP_FAILED);
    memset(table, 0, len);
    bucket_len = len;
    printf(">> table buckets %lu len %lu\n",
            nbuckets, len);
}

bool get(long key, struct entry **e) {
    ul hashh = make_hash(key);
    ul mask = nbuckets-1;
    ul bidx = mask & hashh;
    struct bucket *b = &table[bidx];
    long i;

    //printf("%s hash %16lx mask %16lx bidx %ld\n",
            //__func__, hashh, mask, bidx);

    for (i = 0; i < ENTRIES_PER_BUCKET; i++)
        if (b->entries[i].key == key)
            break;

    bool ret = false;

    if (i < ENTRIES_PER_BUCKET) {
        *e = &b->entries[i];
        ret = true;
    }

    return ret;
}

bool put(long key, long addr, long size) {
    ul hashh = make_hash(key);
    ul mask = nbuckets-1;
    ul bidx = mask & hashh;
    struct bucket *b = &table[bidx];
    long i, inv = -1l;

    //printf("%s hash %16lx mask %16lx bidx %ld\n",
            //__func__, hashh, mask, bidx);

    for (i = 0; i < ENTRIES_PER_BUCKET; i++) {
        if (b->entries[i].key == key) {
            break;
        } else if (b->entries[i].key == 0l) {
            if (inv < 0l)
                inv = i;
        }
    }

    bool ret = false;

    // found existing, overwrite it
    if (i < ENTRIES_PER_BUCKET) {
        b->entries[i].key  = key;
        b->entries[i].addr = addr;
        b->entries[i].size = size;
        ret = true;
    }

    // not exist, but free slot found
    else if (inv >= 0l) {
        b->entries[inv].key  = key;
        b->entries[inv].addr = addr;
        b->entries[inv].size = size;
        ret = true;
    }

    return ret;
}

bool del(long key) {
    ul hashh = make_hash(key);
    ul mask = nbuckets-1;
    ul bidx = mask & hashh;
    struct bucket *b = &table[bidx];
    long i;

    //printf("%s hash %16lx mask %16lx bidx %ld\n",
            //__func__, hashh, mask, bidx);

    for (i = 0; i < ENTRIES_PER_BUCKET; i++)
        if (b->entries[i].key == key)
            break;

    bool ret = false;

    if (i < ENTRIES_PER_BUCKET) {
        memset(&b->entries[i], 0, sizeof(b->entries[0]));
        ret = true;
    }

    return ret;
}

void replay() {
    long missing_frees = 0;
    long itern = 0ul;

    init_table(1ul<<16);

    char buf[64];
    while (fgets(buf, 64, stdin)) {
        long tsc;
        char op;
        assert(2 == sscanf(buf, "%ld %c", &tsc, &op));

        if ('m' == op) {
do_malloc:;
            //printf("%s", buf);
            long size, key;
            assert(3 == sscanf(buf, "%ld m %ld %lx",
                        &tsc,&size,&key));
            void *p = malloc(size);
            assert(p);
            touch(p, size);
            cur_alloc += size;
            put(key, (long)p, size);
        }
        
        else if ('c' == op) {
            //printf("%s", buf);
            long size, n, key;
            assert(4 == sscanf(buf, "%ld c %ld %ld %lx",
                        &tsc,&n,&size,&key));
            void *p = calloc(n,size);
            assert(p);
            touch(p, n*size);
            cur_alloc += n*size;
            assert(put(key, (long)p, n*size));
        }

        else if ('f' == op) {
            //printf("%s", buf);
            long key;
            if (strstr(buf, "nil"))
                continue;
            assert(2 == sscanf(buf, "%ld f %lx",
                        &tsc,&key));
            struct entry *e;
            if (get(key, &e)) {
                long p = e->addr;
                free((void*)p);
                cur_alloc -= e->size;
                assert(del(key));
            } else missing_frees++;
        }

        else if ('r' == op) {
            //printf("%s", buf);
            if (strstr(buf, "nil")) {
                // shift out the (nil) and treat like malloc
                char *pos = strchr(buf, '(');
                assert(pos);
                // +1 for NUL byte; 6 for '(nil) '
                long n = 1+strlen((char*)((long)pos+6));
                memmove(pos, (char*)((long)pos+6), n);
                *strchr(buf, 'r') = 'm';
                goto do_malloc;
            } else {
                long key, size, newkey;
                assert(4 == sscanf(buf, "%ld r %lx %ld %lx",
                            &tsc,&key,&size,&newkey));
                // handle as free then alloc
                struct entry *e;
                if (get(key, &e)) {
                    long p = e->addr;
                    free((void*)p);
                    cur_alloc -= e->size;
                    assert(del(key));
                    assert((p = (long)malloc(size)));
                    touch((void*)p, size);
                    cur_alloc += size;
                    assert(put(newkey, p, size));
                } else missing_frees++;
            }
        }

        else abort();

        if (0 == (itern++ % print_every))
            print_mem();
    }
    printf("missing frees: %ld\n", missing_frees);
}

int main() {
    replay();
    return EXIT_SUCCESS;
}
