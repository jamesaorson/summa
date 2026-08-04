/* ref_count.h is included FIRST, before test.h, on purpose: it makes this file
 * a standing check that the header is self-contained. Include test.h first and
 * ref_count.h silently borrows <stdlib.h> from it, hiding a missing include
 * from every translation unit that reaches for ref_count.h on its own.
 *
 * SUMMA_REF_COUNT_THREADSAFE is deliberately left undefined here so the plain
 * size_t counter gets compiled; ref_count.threaded.test.c covers the atomic one. */
#define SUMMA_REF_COUNT_IMPLEMENTATION
#define SUMMA_REF_COUNT_DEBUG
#include <summa/ref_count/ref_count.h>
#include <summa/ref_count/ref_count.h> /* verify include guard */

#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

/* An owning payload needs a type that owns something. SummaString is the
 * smallest one in the repo, and the destructor case that matters. */
#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <stdint.h>
#include <string.h>

typedef struct {
    int    id;
    double weight;
    char   name[32];
} Payload;

/* Big enough that an undersized allocation runs off the end of its malloc
 * size class instead of landing in padding, so ASan is sure to see it. */
typedef struct {
    unsigned char data[4096];
} BigPayload;

static RefCount identity(RefCount rc) {
    return rc;
}

void test_ref_count_make_returns_live_reference() {
    RefCount rc = ref_count_make(int);
    SUMMA_TEST_ASSERT_NOT_NULL(rc);
    /* A freshly-made reference is already owned by the caller, so it starts at
     * one: a single release must be enough to destroy it. */
    SUMMA_TEST_ASSERT_EQ(1u, rc->count);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

void test_ref_count_as_points_past_the_header() {
    RefCount rc    = ref_count_make(int);
    int*     value = ref_count_as(rc, int*);
    /* Exact, not >=: the payload begins where the header ends. Sizing the
     * allocation off the handle (sizeof(RefCount)) rather than the struct only
     * looks right while the struct happens to be one pointer wide. */
    SUMMA_TEST_ASSERT_EQ((unsigned char*)value, (unsigned char*)rc + sizeof(struct RefCount_t));
    ref_count_release(rc);
}

void test_ref_count_payload_is_readable_and_writable() {
    RefCount rc    = ref_count_make(int);
    int*     value = ref_count_as(rc, int*);
    *value         = 42;
    SUMMA_TEST_ASSERT_EQ(42, *ref_count_as(rc, int*));
    ref_count_release(rc);
}

void test_ref_count_payload_has_room_for_whole_type() {
    RefCount rc      = ref_count_make(Payload);
    Payload* payload = ref_count_as(rc, Payload*);
    memset(payload, 0, sizeof(Payload));
    payload->id     = 7;
    payload->weight = 1.5;
    memcpy(payload->name, "summa", sizeof("summa"));
    SUMMA_TEST_ASSERT_EQ(7, payload->id);
    SUMMA_TEST_ASSERT_EQ(1.5, payload->weight);
    SUMMA_TEST_ASSERT_EQ_STR("summa", payload->name);
    ref_count_release(rc);
}

void test_ref_count_large_payload_is_fully_writable() {
    RefCount    rc      = ref_count_make(BigPayload);
    BigPayload* payload = ref_count_as(rc, BigPayload*);
    memset(payload->data, 0xA5, sizeof(payload->data));
    for (size_t i = 0; i < sizeof(payload->data); i++) {
        SUMMA_TEST_ASSERT_EQ(0xA5, payload->data[i]);
    }
    ref_count_release(rc);
}

void test_ref_count_payload_is_suitably_aligned() {
    RefCount rc      = ref_count_make(Payload);
    void*    payload = ref_count_as(rc, void*);
    /* ref_count_make hands back storage for an arbitrary caller-chosen type, so
     * like malloc it owes that storage the strictest alignment any type can
     * ask for. A payload parked at header-width offset only satisfies types no
     * stricter than the header itself. */
    SUMMA_TEST_ASSERT_EQ(0u, (uintptr_t)payload % alignof(max_align_t));
    ref_count_release(rc);
}

void test_ref_count_acquire_increments_and_returns_same_reference() {
    RefCount rc       = ref_count_make(int);
    RefCount acquired = ref_count_acquire(rc);
    SUMMA_TEST_ASSERT_EQ(rc, acquired);
    SUMMA_TEST_ASSERT_EQ(2u, rc->count);
    ref_count_release(rc);
    ref_count_release(rc);
}

void test_ref_count_release_keeps_reference_alive_while_owners_remain() {
    RefCount rc    = ref_count_make(int);
    int*     value = ref_count_as(rc, int*);
    *value         = 99;
    ref_count_acquire(rc);
    RefCount released = ref_count_release(rc);
    /* One owner is left, so release hands the reference back rather than
     * destroying it -- and the payload is untouched. */
    SUMMA_TEST_ASSERT_EQ(rc, released);
    SUMMA_TEST_ASSERT_EQ(1u, rc->count);
    SUMMA_TEST_ASSERT_EQ(99, *ref_count_as(rc, int*));
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

void test_ref_count_release_of_last_owner_returns_null() {
    RefCount rc = ref_count_make(int);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

void test_ref_count_acquires_and_releases_balance() {
    RefCount rc = ref_count_make(int);
    for (size_t i = 0; i < 16; i++) {
        ref_count_acquire(rc);
    }
    SUMMA_TEST_ASSERT_EQ(17u, rc->count);
    for (size_t i = 0; i < 16; i++) {
        SUMMA_TEST_ASSERT_EQ(rc, ref_count_release(rc));
    }
    SUMMA_TEST_ASSERT_EQ(1u, rc->count);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

void test_ref_count_independent_references_do_not_share_state() {
    RefCount first  = ref_count_make(int);
    RefCount second = ref_count_make(int);
    SUMMA_TEST_ASSERT_NEQ(first, second);
    *ref_count_as(first, int*)  = 1;
    *ref_count_as(second, int*) = 2;
    ref_count_acquire(first);
    SUMMA_TEST_ASSERT_EQ(2u, first->count);
    SUMMA_TEST_ASSERT_EQ(1u, second->count);
    SUMMA_TEST_ASSERT_EQ(1, *ref_count_as(first, int*));
    SUMMA_TEST_ASSERT_EQ(2, *ref_count_as(second, int*));
    ref_count_release(first);
    ref_count_release(first);
    ref_count_release(second);
}

void test_ref_count_make_composes_as_an_expression() {
    /* A macro carrying its own trailing semicolon parses only as a standalone
     * initializer. These two shapes are what catch that. */
    RefCount passed = identity(ref_count_make(int));
    SUMMA_TEST_ASSERT_NOT_NULL(passed);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(passed));

    RefCount first = ref_count_make(int), second = ref_count_make(int);
    SUMMA_TEST_ASSERT_NEQ(first, second);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(first));
    SUMMA_TEST_ASSERT_NULL(ref_count_release(second));
}

void test_ref_count_scoped_restores_the_count() {
    RefCount rc       = ref_count_make(int);
    size_t   observed = 0;
    ref_count_scoped(rc, { observed = rc->count; });
    /* The scope borrows a reference for the duration of the body and gives it
     * back on exit, leaving the caller's own reference intact. */
    SUMMA_TEST_ASSERT_EQ(2u, observed);
    SUMMA_TEST_ASSERT_EQ(1u, rc->count);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

void test_ref_count_scoped_keeps_payload_alive() {
    RefCount rc             = ref_count_make(int);
    *ref_count_as(rc, int*) = 5;
    ref_count_scoped(rc, {
        int* value = ref_count_as(rc, int*);
        *value += 1;
    });
    SUMMA_TEST_ASSERT_EQ(6, *ref_count_as(rc, int*));
    ref_count_release(rc);
}

void test_ref_count_scoped_inside_a_branch() {
    RefCount rc       = ref_count_make(int);
    bool     body_ran = false;
    bool     enter    = true;
    /* The braces are load-bearing. ref_count_scoped expands to a bare statement
     * sequence, so an unbraced `if (enter) ref_count_scoped(...)` would put
     * only the acquire under the branch and let the body and the matching
     * release run unconditionally. That shape doesn't need a runtime assertion
     * to catch it -- GCC rejects it outright under -Werror=multistatement-macros
     * -- so what is left to check here is that the braced form still balances. */
    if (enter) {
        ref_count_scoped(rc, { body_ran = true; });
    }
    SUMMA_TEST_ASSERT(body_ran);
    SUMMA_TEST_ASSERT_NOT_NULL(rc);
    SUMMA_TEST_ASSERT_EQ(1u, rc->count);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

/* ── Destructors ──────────────────────────────────────────────────────────
 *
 * Without one, a reference is released as raw bytes and anything the payload
 * owns is abandoned. These pin the three properties that makes safe: it runs
 * exactly once, it runs only on the release that reaches zero, and it gets the
 * payload rather than the header. */

static size_t destructor_calls  = 0;
static void*  destructor_target = nullptr;

static void count_destructor(void* payload) {
    destructor_calls++;
    destructor_target = payload;
}

static void reset_destructor_spy(void) {
    destructor_calls  = 0;
    destructor_target = nullptr;
}

void test_ref_count_destructor_runs_on_final_release() {
    reset_destructor_spy();
    RefCount rc = ref_count_make_with_destructor(int, count_destructor);
    SUMMA_TEST_ASSERT_EQ(0u, destructor_calls);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
    SUMMA_TEST_ASSERT_EQ(1u, destructor_calls);
}

/* The whole point of hanging it off the zero transition: an owner letting go
 * while others remain must not tear down what they are still using. */
void test_ref_count_destructor_does_not_run_while_owners_remain() {
    reset_destructor_spy();
    RefCount rc = ref_count_make_with_destructor(int, count_destructor);
    ref_count_acquire(rc);
    ref_count_acquire(rc);

    SUMMA_TEST_ASSERT_EQ(rc, ref_count_release(rc));
    SUMMA_TEST_ASSERT_EQ(0u, destructor_calls);
    SUMMA_TEST_ASSERT_EQ(rc, ref_count_release(rc));
    SUMMA_TEST_ASSERT_EQ(0u, destructor_calls);

    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
    SUMMA_TEST_ASSERT_EQ(1u, destructor_calls);
}

/* The header is what `rc` points at; the destructor is handed what the caller
 * asked for storage for. Getting this backwards type-checks and corrupts. */
void test_ref_count_destructor_receives_the_payload() {
    reset_destructor_spy();
    RefCount rc       = ref_count_make_with_destructor(Payload, count_destructor);
    void*    expected = ref_count_as(rc, void*);
    ref_count_release(rc);
    SUMMA_TEST_ASSERT_EQ(expected, destructor_target);
}

/* A null hook is the pre-destructor behavior, and ref_count_make must keep
 * giving exactly that. */
void test_ref_count_make_leaves_the_destructor_null() {
    RefCount rc = ref_count_make(int);
    SUMMA_TEST_ASSERT_NULL(rc->destroy);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

void test_ref_count_destructor_survives_acquire_release_churn() {
    reset_destructor_spy();
    RefCount rc = ref_count_make_with_destructor(int, count_destructor);
    for (size_t i = 0; i < 16; i++) {
        ref_count_acquire(rc);
    }
    for (size_t i = 0; i < 16; i++) {
        SUMMA_TEST_ASSERT_EQ(rc, ref_count_release(rc));
    }
    SUMMA_TEST_ASSERT_EQ(0u, destructor_calls);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
    SUMMA_TEST_ASSERT_EQ(1u, destructor_calls);
}

/* The case the feature exists for, and the one silently broken before it: a
 * payload that is itself a handle. The bare free released the SummaString
 * handle's storage and abandoned its elements buffer -- a leak ASan sees on
 * Linux and `leaks` sees here. */
static void free_scheme_string(void* payload) {
    summa_string_free(*(SummaString*)payload);
}

void test_ref_count_destructor_releases_an_owning_payload() {
    RefCount rc                     = ref_count_make_with_destructor(SummaString, free_scheme_string);
    *ref_count_as(rc, SummaString*) = summa_string_make("hello");
    SUMMA_TEST_ASSERT_EQ_STR("hello", (*ref_count_as(rc, SummaString*))->value);
    SUMMA_TEST_ASSERT_NULL(ref_count_release(rc));
}

/* Reentrancy: a destructor releasing another reference is the environment /
 * closure case this was built for, so it has to work rather than merely not
 * crash. The inner reference is owned by the outer one's payload. */
static void release_inner(void* payload) {
    destructor_calls++;
    ref_count_release(*(RefCount*)payload);
}

void test_ref_count_destructor_may_release_another_reference() {
    reset_destructor_spy();
    RefCount inner = ref_count_make_with_destructor(int, count_destructor);

    RefCount outer                  = ref_count_make_with_destructor(RefCount, release_inner);
    *ref_count_as(outer, RefCount*) = inner;

    /* Outer's teardown runs, which runs inner's: two calls through one
     * release, and neither reference outlives it. */
    SUMMA_TEST_ASSERT_NULL(ref_count_release(outer));
    SUMMA_TEST_ASSERT_EQ(2u, destructor_calls);
}

int main(int argc, char** argv) {
    summa_test_begin("ref_count", argc, argv);
    SUMMA_TEST_RUN(test_ref_count_make_returns_live_reference);
    SUMMA_TEST_RUN(test_ref_count_as_points_past_the_header);
    SUMMA_TEST_RUN(test_ref_count_payload_is_readable_and_writable);
    SUMMA_TEST_RUN(test_ref_count_payload_has_room_for_whole_type);
    SUMMA_TEST_RUN(test_ref_count_large_payload_is_fully_writable);
    SUMMA_TEST_RUN(test_ref_count_payload_is_suitably_aligned);
    SUMMA_TEST_RUN(test_ref_count_acquire_increments_and_returns_same_reference);
    SUMMA_TEST_RUN(test_ref_count_release_keeps_reference_alive_while_owners_remain);
    SUMMA_TEST_RUN(test_ref_count_release_of_last_owner_returns_null);
    SUMMA_TEST_RUN(test_ref_count_acquires_and_releases_balance);
    SUMMA_TEST_RUN(test_ref_count_independent_references_do_not_share_state);
    SUMMA_TEST_RUN(test_ref_count_make_composes_as_an_expression);
    SUMMA_TEST_RUN(test_ref_count_scoped_restores_the_count);
    SUMMA_TEST_RUN(test_ref_count_scoped_keeps_payload_alive);
    SUMMA_TEST_RUN(test_ref_count_scoped_inside_a_branch);

    SUMMA_TEST_RUN(test_ref_count_destructor_runs_on_final_release);
    SUMMA_TEST_RUN(test_ref_count_destructor_does_not_run_while_owners_remain);
    SUMMA_TEST_RUN(test_ref_count_destructor_receives_the_payload);
    SUMMA_TEST_RUN(test_ref_count_make_leaves_the_destructor_null);
    SUMMA_TEST_RUN(test_ref_count_destructor_survives_acquire_release_churn);
    SUMMA_TEST_RUN(test_ref_count_destructor_releases_an_owning_payload);
    SUMMA_TEST_RUN(test_ref_count_destructor_may_release_another_reference);
    return summa_test_end();
}
