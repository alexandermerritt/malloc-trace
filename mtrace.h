#pragma once

#include <stdint.h>

#define likely(expr)     __builtin_expect(expr, 1)
#define unlikely(expr)   __builtin_expect(expr, 0)

typedef unsigned long ul;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t u8;

static inline
u64 rdtsc(void)
{
    u32 low, high;
    asm volatile("rdtsc" : "=a" (low), "=d" (high));
    return ((u64)low) | (((u64)high) << 32);
}

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

// TODO
int posix_memalign(void **memptr, size_t alignment, size_t size);
void *aligned_alloc(size_t alignment, size_t size);
void *valloc(size_t size);

// obsolete
void *memalign(size_t alignment, size_t size);

enum {
    OP_MALLOC = 1,
    OP_FREE,
    OP_CALLOC,
    OP_REALLOC
};

// two of these fit into one cache line
struct entry {
    union {
        struct {
            size_t size;
            void *ptr;
        } ma;
        struct {
            void *ptr;
        } fr;
        struct {
            size_t nmemb;
            size_t size;
            void *ret;
        } ca;
        struct {
            void *ptr;
            size_t size;
            void *ret;
        } re;
    } args;
    u64 tsc : 48;
    u8 op : 3;
} __attribute__((packed,aligned(8)));

