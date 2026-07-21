#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

/* The single-threaded cases in ref_count.test.c build the header without
 * SUMMA_REF_COUNT_THREADSAFE, so between the two files both configurations of
 * the count field get compiled and exercised. */
#define SUMMA_REF_COUNT_IMPLEMENTATION
#define SUMMA_REF_COUNT_THREADSAFE
#include <summa/ref_count/ref_count.h>

#define THREAD_COUNT 8
#define CHURN_ITERATIONS 10000

typedef struct {
    RefCount rc;
    bool     saw_early_free;
    bool     saw_wrong_reference;
} ChurnArgs;

static SUMMA_TEST_THREAD_RESULT churn_thread(void* arg) {
    ChurnArgs* args = (ChurnArgs*)arg;
    for (size_t i = 0; i < CHURN_ITERATIONS; i++) {
        if (ref_count_acquire(args->rc) != args->rc) {
            args->saw_wrong_reference = true;
        }
        if (ref_count_release(args->rc) == nullptr) {
            args->saw_early_free = true;
        }
    }
    return SUMMA_TEST_THREAD_DONE;
}

void test_ref_count_concurrent_churn_balances() {
    RefCount            rc = ref_count_make(int);
    summa_test_thread_t threads[THREAD_COUNT];
    ChurnArgs           args[THREAD_COUNT] = {};

    /* Main holds a reference for the whole run, so no thread's release can ever
     * be the last one -- a nullptr coming back means an increment was lost. */
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        args[i].rc = rc;
        summa_test_thread_start(&threads[i], churn_thread, &args[i]);
    }
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        summa_test_thread_join(&threads[i]);
        SUMMA_TEST_ASSERT(!args[i].saw_early_free);
        SUMMA_TEST_ASSERT(!args[i].saw_wrong_reference);
    }

    /* Every acquire was paired with a release. A torn read-modify-write shows
     * up here as a count that drifted off main's one remaining reference. */
    SUMMA_TEST_ASSERT_EQ(1u, rc->count);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

typedef struct {
    RefCount rc;
    bool     freed;
} ReleaseArgs;

static SUMMA_TEST_THREAD_RESULT release_thread(void* arg) {
    ReleaseArgs* args = (ReleaseArgs*)arg;
    args->freed       = ref_count_release(args->rc) == nullptr;
    return SUMMA_TEST_THREAD_DONE;
}

void test_ref_count_concurrent_final_release_frees_exactly_once() {
    RefCount            rc = ref_count_make(int);
    summa_test_thread_t threads[THREAD_COUNT];
    ReleaseArgs         args[THREAD_COUNT] = {};

    for (size_t i = 1; i < THREAD_COUNT; i++) {
        ref_count_acquire(rc);
    }
    SUMMA_TEST_ASSERT_EQ((size_t)THREAD_COUNT, rc->count);

    /* Every owner lets go at once. Exactly one thread must observe the drop to
     * zero: two would be a double free, none would be a leak. Each thread
     * writes only its own slot, so the tally itself races with nothing. */
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        args[i].rc = rc;
        summa_test_thread_start(&threads[i], release_thread, &args[i]);
    }

    size_t freed_count = 0;
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        summa_test_thread_join(&threads[i]);
        freed_count += args[i].freed ? 1u : 0u;
    }
    SUMMA_TEST_ASSERT_EQ(1u, freed_count);
}

typedef struct {
    RefCount rc;
    bool     payload_changed;
} PayloadArgs;

static SUMMA_TEST_THREAD_RESULT payload_reader_thread(void* arg) {
    PayloadArgs* args = (PayloadArgs*)arg;
    for (size_t i = 0; i < CHURN_ITERATIONS; i++) {
        RefCount held = ref_count_acquire(args->rc);
        if (*ref_count_as(held, int*) != 1234) {
            args->payload_changed = true;
        }
        ref_count_release(held);
    }
    return SUMMA_TEST_THREAD_DONE;
}

void test_ref_count_payload_survives_concurrent_ownership() {
    RefCount            rc = ref_count_make(int);
    summa_test_thread_t threads[THREAD_COUNT];
    PayloadArgs         args[THREAD_COUNT] = {};

    *ref_count_as(rc, int*) = 1234;

    /* Nobody writes the payload, so the only way a reader sees something other
     * than 1234 is if the counter let the block be freed and reused underneath
     * it -- which is also a use-after-free ASan should catch first. */
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        args[i].rc = rc;
        summa_test_thread_start(&threads[i], payload_reader_thread, &args[i]);
    }
    for (size_t i = 0; i < THREAD_COUNT; i++) {
        summa_test_thread_join(&threads[i]);
        SUMMA_TEST_ASSERT(!args[i].payload_changed);
    }

    SUMMA_TEST_ASSERT_EQ(1234, *ref_count_as(rc, int*));
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

int main(int argc, char** argv) {
    summa_test_begin("ref_count.threaded", argc, argv);
    SUMMA_TEST_RUN(test_ref_count_concurrent_churn_balances);
    SUMMA_TEST_RUN(test_ref_count_concurrent_final_release_frees_exactly_once);
    SUMMA_TEST_RUN(test_ref_count_payload_survives_concurrent_ownership);
    return summa_test_end();
}
