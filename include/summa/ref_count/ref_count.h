#ifndef SUMMA_REF_COUNT_H
#define SUMMA_REF_COUNT_H

#include <stddef.h>

#ifdef SUMMA_REF_COUNT_THREADSAFE
#include <stdatomic.h>
#endif

/* The payload is carved out of the same allocation, starting one header-width
 * past the base, so the header's alignment is what the payload inherits.
 * ref_count_make takes an arbitrary type, which puts it under malloc's
 * obligation: storage aligned for any object. Aligning the counter to
 * max_align_t rounds the struct's size up to match, so `rc + 1` lands on a
 * suitably aligned address for every T. */
struct RefCount_t {
#ifdef SUMMA_REF_COUNT_THREADSAFE
    alignas(max_align_t) atomic_size_t count;
#else
    alignas(max_align_t) size_t count;
#endif
};
typedef struct RefCount_t* RefCount;

RefCount ref_count_make_impl(size_t type_size);
RefCount ref_count_acquire(RefCount ref_count);
RefCount ref_count_release(RefCount ref_count);

#define ref_count_make(T) ref_count_make_impl(sizeof(T))
#define ref_count_as(rc, T) (T)(rc + 1)
#define ref_count_scoped(rc, body) \
    ref_count_acquire(rc);         \
    do {                           \
        body;                      \
    } while (0);                   \
    rc = ref_count_release(rc);

#endif

#ifdef SUMMA_REF_COUNT_IMPLEMENTATION
#ifndef SUMMA_REF_COUNT_IMPLEMENTATION_ONCE
#define SUMMA_REF_COUNT_IMPLEMENTATION_ONCE

#include <assert.h>
#include <stdlib.h>

#ifdef SUMMA_REF_COUNT_DEBUG
#include <stdio.h>
#endif

RefCount ref_count_make_impl(size_t data_size) {
    RefCount rc = malloc(sizeof(struct RefCount_t) + data_size);
    assert(rc);
    rc->count = 1;
#ifdef SUMMA_REF_COUNT_DEBUG
    fprintf(stderr,
            "ref_count_make_impl: allocated %zu bytes at %p, count=%zu\n",
            sizeof(struct RefCount_t) + data_size,
            (void*)rc,
            rc->count);
#endif
    return rc;
}

RefCount ref_count_acquire(RefCount ref_count) {
    assert(ref_count);
#ifdef SUMMA_REF_COUNT_THREADSAFE
    atomic_fetch_add(&ref_count->count, 1);
#else
    ref_count->count++;
#endif
#ifdef SUMMA_REF_COUNT_DEBUG
    fprintf(stderr, "ref_count_acquire: count=%zu at %p\n", ref_count->count, (void*)ref_count);
#endif
    return ref_count;
}

RefCount ref_count_release(RefCount ref_count) {
#ifdef SUMMA_REF_COUNT_THREADSAFE
    if (atomic_fetch_sub(&ref_count->count, 1) == 1) {
// Free the ref count and the internal data together
#ifdef SUMMA_REF_COUNT_DEBUG
        fprintf(stderr, "ref_count_release: freeing at %p\n", (void*)ref_count);
#endif
        free(ref_count);
        return nullptr;
    }
#ifdef SUMMA_REF_COUNT_DEBUG
    else {
        fprintf(stderr, "ref_count_release: count=%zu at %p\n", ref_count->count, (void*)ref_count);
    }
#endif

#else
    if (ref_count->count > 0) {
        ref_count->count--;
#ifdef SUMMA_REF_COUNT_DEBUG
        fprintf(stderr, "ref_count_release: count=%zu at %p\n", ref_count->count, (void*)ref_count);
#endif
    }
    if (ref_count->count == 0) {
#ifdef SUMMA_REF_COUNT_DEBUG
        fprintf(stderr, "ref_count_release: freeing at %p\n", (void*)ref_count);
#endif
        // Free the ref count and the internal data together
        free(ref_count);
        return nullptr;
    }
#endif
    return ref_count;
}

#endif /* SUMMA_REF_COUNT_IMPLEMENTATION_ONCE */
#endif /* SUMMA_REF_COUNT_IMPLEMENTATION */
