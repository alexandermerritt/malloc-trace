#define _GNU_SOURCE
#include <malloc.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>

#include "mtrace.h"

static u64 start_tsc = 0ul;

static __thread struct entry *entries;
static __thread u64 next;

static __thread bool tracing = true;

#define __MTRACE_BOOTSTRAP_LEN      4096
static __thread u8 bootstrap[__MTRACE_BOOTSTRAP_LEN];
static __thread u8 boff = 0ul;

static struct entry *entry_list[1024];
static ul entry_list_next = 0ul;
static __thread bool registered = false;

static u64 max_entries = 1ul<<20;

static void* (*m)(size_t);
static void  (*f)(void*);
static void* (*c)(size_t,size_t);
static void* (*r)(void*,size_t);

static const char log_fname[] = "mtrace.log";

__attribute__((constructor))
void init() {
    tracing = false;
    start_tsc = rdtsc();
    m = dlsym(RTLD_NEXT, "malloc");
    f = dlsym(RTLD_NEXT, "free");
    c = dlsym(RTLD_NEXT, "calloc");
    r = dlsym(RTLD_NEXT, "realloc");
    if (!(m && (m != malloc))) abort();
    if (!(f && (f != free))) abort();
    if (!(c && (c != calloc))) abort();
    if (!(r && (r != realloc))) abort();
    tracing = true;
};

static inline
void init_entries() {
    if (unlikely(!entries)) {
        if (unlikely(!c)) init();
        assert(c != NULL);
        entries = c(max_entries, sizeof(*entries));
        assert(entries);
        next = 0ul;
    }
}

static
void register_self() {
    if (registered) abort();
    registered = true;
    if (!entries)
        init_entries();
    ul idx = __atomic_fetch_add(&entry_list_next, 1ul, __ATOMIC_SEQ_CST);
    entry_list[idx] = entries;
}

static FILE *fp = NULL;
static pthread_mutex_t fplock = PTHREAD_MUTEX_INITIALIZER;

static inline
void open_log() {
    if (0 == pthread_mutex_trylock(&fplock)) {
        if (!fp && !(fp = fopen(log_fname, "w"))) {
            perror("fopen log");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_unlock(&fplock);
    }
    // if we didn't get the lock, wait here until fp is set
    while (fp == NULL)
        ;
}

static
void dump_entries() {
    assert(entries);
    bool orig = tracing;
    tracing = false;
    if (unlikely(!fp))
        open_log();
    const size_t len = next * sizeof(*entries);
    const size_t nb = fwrite(entries, 1, len, fp);
    if (nb != len) {
        perror("fwrite");
        exit(EXIT_FAILURE);
    }
    tracing = orig;
}

static
void try_flush() {
    if (unlikely(!registered))
        register_self();
    if (unlikely(next >= max_entries)) {
        dump_entries();
        next = 0ul;
    }
}

__attribute__((destructor))
void fin() {
    tracing = false;
    if (!fp) open_log();
    // flush each thread's remaining entries
    ul nentries = __atomic_load_n(&entry_list_next, __ATOMIC_SEQ_CST);
    for (ul e = 0; e < nentries; e++) {
        // print up to (incl.) entry i where tsc[i] > tsc[i+1]
        // as that is where that thread's next would be pointing
        ul i;
        for (i = 0ul; i < (max_entries-1); i++)
            if (entry_list[e][i].tsc >
                    entry_list[e][i+1].tsc)
                break;
        const size_t len = i * sizeof(*entries);
        const size_t nb = fwrite(entry_list[e], 1, len, fp);
        if (nb != len) {
            perror("fwrite");
            exit(EXIT_FAILURE);
        }
    }
}

void* malloc(size_t size) {
    // constructor for ld.so may require malloc, but if you preload
    // us, we cannot dlsym malloc b/c ld hasn't loaded libc yet.. so
    // we bootstrap malloc with some static buffer
    if (unlikely(!m)) {
        if (!((boff+size) < __MTRACE_BOOTSTRAP_LEN)) abort();
        void *p = (void*)((ptrdiff_t)bootstrap + boff);
        boff += size;
        return p;
    }
    if (unlikely(!tracing))
        return m(size);
    tracing = false;

    init_entries();
    try_flush();
    struct entry *e = &entries[next++];
    e->tsc = rdtsc()-start_tsc;
    e->op = OP_MALLOC;
    e->args.ma.size = size;
    e->args.ma.ptr = m(size);

    tracing = true;

    return e->args.ma.ptr;
}

void free(void *ptr) {
    // avoid "freeing" buffer from static area
    if (unlikely(ptr >= (void*)bootstrap
                && ((ptrdiff_t)ptr-(ptrdiff_t)bootstrap)
                        <= __MTRACE_BOOTSTRAP_LEN))
        return;
    if (unlikely(!f)) init();
    if (unlikely(!tracing))
        return f(ptr);
    tracing = false;

    init_entries();
    try_flush();
    struct entry *e = &entries[next++];
    e->tsc = rdtsc()-start_tsc;
    e->op = OP_FREE;
    e->args.fr.ptr = ptr;

    tracing = true;

    f(ptr);
}

void* calloc(size_t nmemb, size_t size) {
    if (unlikely(!c)) {
        if (!((boff+(nmemb*size)) < 1024)) abort();
        void *p = (void*)((ptrdiff_t)bootstrap + boff);
        boff += (nmemb*size);
        return p;
    }
    if (unlikely(!tracing))
        return c(nmemb,size);
    tracing = false;

    init_entries();
    try_flush();
    struct entry *e = &entries[next++];
    e->tsc = rdtsc()-start_tsc;
    e->op = OP_CALLOC;
    e->args.ca.nmemb = nmemb;
    e->args.ca.size = size;
    e->args.ca.ret = c(nmemb,size);

    tracing = true;

    return e->args.ca.ret;
}

void* realloc(void *ptr, size_t size) {
    if (unlikely(!r)) init();
    if (unlikely(!tracing))
        return r(ptr,size);
    tracing = false;

    init_entries();
    try_flush();
    struct entry *e = &entries[next++];
    e->tsc = rdtsc()-start_tsc;
    e->op = OP_REALLOC;
    e->args.re.ptr = ptr;
    e->args.re.size = size;
    e->args.re.ret = r(ptr,size);

    tracing = true;

    return e->args.re.ret;
}

