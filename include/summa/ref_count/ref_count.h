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

/* Runs just before the block is freed, and only on the release that drops the
 * count to zero. Receives the payload -- the `rc + 1` pointer -- never the
 * header.
 *
 * Without one, a reference is released as raw bytes, so anything the payload
 * owns leaks: a refcounted SummaString frees its handle and abandons its
 * elements buffer. The destructor is what lets a reference own data rather
 * than merely carry it.
 *
 * Since the payload is the caller's own type, a destructor for a type that is
 * itself a handle takes one line of unwrapping:
 *
 *     static void free_string(void* p) { summa_string_free(*(SummaString*)p); }
 *
 * Reentrancy is supported: a destructor may release other references, which is
 * the case this exists for -- an environment owning bindings whose values own
 * environments. The recursion is the caller's own object graph, so its depth is
 * theirs to bound; a cyclic graph will not terminate, and refcounting cannot
 * reclaim one anyway. */
typedef void (*RefCountDestructor)(void*);

struct RefCount_t {
#ifdef SUMMA_REF_COUNT_THREADSAFE
    alignas(max_align_t) atomic_size_t count;
#else
    alignas(max_align_t) size_t count;
#endif
    /* Null means "no teardown beyond the free", which is what ref_count_make
     * gives and what every reference did before destructors existed.
     *
     * Placing it after the counter grows the header, and therefore moves the
     * payload -- which is exactly why the payload offset is derived from
     * `sizeof(struct RefCount_t)` rather than written down as a number.
     * test_ref_count_as_points_past_the_header pins that. */
    RefCountDestructor destroy;
};
typedef struct RefCount_t* RefCount;

RefCount ref_count_make_impl(size_t type_size);
RefCount ref_count_make_with_destructor_impl(size_t type_size, RefCountDestructor destroy);
RefCount ref_count_acquire(RefCount ref_count);
RefCount ref_count_release(RefCount ref_count);

#define ref_count_make(T) ref_count_make_impl(sizeof(T))
#define ref_count_make_with_destructor(T, destroy) ref_count_make_with_destructor_impl(sizeof(T), (destroy))
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

RefCount ref_count_make_with_destructor_impl(size_t data_size, RefCountDestructor destroy) {
    RefCount rc = malloc(sizeof(struct RefCount_t) + data_size);
    assert(rc);
    rc->count   = 1;
    rc->destroy = destroy;
#ifdef SUMMA_REF_COUNT_DEBUG
    fprintf(stderr,
            "ref_count_make_impl: allocated %zu bytes at %p, count=%zu\n",
            sizeof(struct RefCount_t) + data_size,
            (void*)rc,
            rc->count);
#endif
    return rc;
}

/* A reference with nothing to tear down is the destructor-carrying one with a
 * null hook, so there is a single allocation path to keep correct. */
RefCount ref_count_make_impl(size_t data_size) {
    return ref_count_make_with_destructor_impl(data_size, nullptr);
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

/* Teardown, on the one branch that has already decided to free. Running the
 * destructor here rather than in either caller is what gives it the same
 * exactly-once guarantee the free has: under SUMMA_REF_COUNT_THREADSAFE the
 * atomic fetch-sub elects a single winner, and the winner is who gets here. */
static void ref_count_destroy(RefCount ref_count) {
#ifdef SUMMA_REF_COUNT_DEBUG
    fprintf(stderr, "ref_count_release: freeing at %p\n", (void*)ref_count);
#endif
    if (ref_count->destroy) {
        /* The payload, not the header -- and before the free, while it is still
         * there to be torn down. */
        ref_count->destroy(ref_count + 1);
    }
    /* Free the ref count and the internal data together */
    free(ref_count);
}

RefCount ref_count_release(RefCount ref_count) {
#ifdef SUMMA_REF_COUNT_THREADSAFE
    if (atomic_fetch_sub(&ref_count->count, 1) == 1) {
        ref_count_destroy(ref_count);
        return nullptr;
    }
#ifdef SUMMA_REF_COUNT_DEBUG
    else {
        fprintf(stderr, "ref_count_release: count=%zu at %p\n", ref_count->count, (void*)ref_count);
    }
#endif

#else
    /* The free hangs off the decrement that reached zero, rather than off a
     * second look at the counter afterwards. Same outcome for every balanced
     * release, and it keeps an over-release -- a count already at zero, which
     * is a use-after-free the caller has already committed -- from running the
     * destructor a second time. */
    if (ref_count->count > 0) {
        ref_count->count--;
#ifdef SUMMA_REF_COUNT_DEBUG
        fprintf(stderr, "ref_count_release: count=%zu at %p\n", ref_count->count, (void*)ref_count);
#endif
        if (ref_count->count == 0) {
            ref_count_destroy(ref_count);
            return nullptr;
        }
    }
#endif
    return ref_count;
}

#endif /* SUMMA_REF_COUNT_IMPLEMENTATION_ONCE */
#endif /* SUMMA_REF_COUNT_IMPLEMENTATION */
