#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <string.h>

/* Nothing here is scoped, and that is the specification rather than an
 * oversight: an interned name belongs to a process-wide table with no way to
 * empty it, because every symbol in every live value points into it. See the
 * intern table's comment in scheme.h. What the cases below pin is that the
 * table hands back one record per distinct name and allocates for nothing
 * else. */

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

/* Distinct across the whole file, since the table survives from one case to the
 * next within a run. */
#define UNIQUE(suffix) "summa-test-intern-" suffix

void test_symbols_the_same_name_interns_to_the_same_pointer() {
    /* Two calls, two separate `const char*` arguments, one record. */
    char copy[64];
    strcpy(copy, UNIQUE("same"));

    const SummaSchemeSymbolName first  = summa_scheme_symbol_intern(UNIQUE("same"));
    const SummaSchemeSymbolName second = summa_scheme_symbol_intern(copy);

    SUMMA_TEST_ASSERT_NOT_NULL(first);
    SUMMA_TEST_ASSERT_EQ(first, second);
    SUMMA_TEST_ASSERT_EQ_STR(UNIQUE("same"), first->value);
    SUMMA_TEST_ASSERT_EQ(strlen(UNIQUE("same")), first->length);
}

void test_symbols_distinct_names_intern_to_distinct_pointers() {
    const SummaSchemeSymbolName left  = summa_scheme_symbol_intern(UNIQUE("left"));
    const SummaSchemeSymbolName right = summa_scheme_symbol_intern(UNIQUE("right"));

    SUMMA_TEST_ASSERT_NEQ(left, right);
}

/* The claim the whole change rests on: a name already in the table costs
 * nothing to name again. */
void test_symbols_reinterning_a_name_does_not_grow_the_table() {
    /* The evaluator interns its own vocabulary -- the special forms, the
     * builtins -- on the first intern of anything, and each case is its own
     * process under CTest. Getting that out of the way first is what makes the
     * reading below a difference of one rather than of forty. */
    summa_scheme_symbol_intern(UNIQUE("grow-first"));

    const size_t before = summa_scheme_symbol_interned_count();

    summa_scheme_symbol_intern(UNIQUE("grow"));
    const size_t after_first = summa_scheme_symbol_interned_count();
    SUMMA_TEST_ASSERT_EQ(before + 1, after_first);

    for (size_t i = 0; i < 1000; i++) {
        summa_scheme_symbol_intern(UNIQUE("grow"));
    }
    SUMMA_TEST_ASSERT_EQ(after_first, summa_scheme_symbol_interned_count());
}

/* A thousand distinct names is several bucket-array doublings. A record's own
 * address has to survive every one of them, or a symbol interned before a
 * growth would name freed memory afterwards. */
void test_symbols_a_growth_does_not_move_a_record() {
    char                  name[64];
    SummaSchemeSymbolName held[1000];

    for (size_t i = 0; i < 1000; i++) {
        snprintf(name, sizeof(name), UNIQUE("stable-%zu"), i);
        held[i] = summa_scheme_symbol_intern(name);
    }

    for (size_t i = 0; i < 1000; i++) {
        snprintf(name, sizeof(name), UNIQUE("stable-%zu"), i);
        SUMMA_TEST_ASSERT_EQ(held[i], summa_scheme_symbol_intern(name));
        SUMMA_TEST_ASSERT_EQ_STR(name, held[i]->value);
    }
}

/* Every name a program mentions goes through the reader, so this is the main
 * intern site and the one a per-call cost is measured against. */
void test_symbols_reading_the_same_source_twice_interns_once() {
    SCOPED_GLOBAL_ENV(env) {
        const char* const source = "(" UNIQUE("read-head") " " UNIQUE("read-tail") ")";

        SummaSchemeValue first = {};
        SUMMA_TEST_ASSERT(!summa_scheme_read(env, source, nullptr, &first).had);
        const size_t after_first = summa_scheme_symbol_interned_count();

        SummaSchemeValue second = {};
        SUMMA_TEST_ASSERT(!summa_scheme_read(env, source, nullptr, &second).had);
        SUMMA_TEST_ASSERT_EQ(after_first, summa_scheme_symbol_interned_count());

        /* Two reads, two independent lists, and the same two records inside
         * them. */
        SUMMA_TEST_ASSERT_NEQ(first.value.list.value, second.value.list.value);
        SUMMA_TEST_ASSERT_EQ(first.value.list.value->value[0].value.symbol.value,
                             second.value.list.value->value[0].value.symbol.value);
        SUMMA_TEST_ASSERT_EQ(first.value.list.value->value[1].value.symbol.value,
                             second.value.list.value->value[1].value.symbol.value);

        summa_scheme_value_free(&first);
        summa_scheme_value_free(&second);
    }
}

/* Copying a symbol copies the pointer, and freeing one gives nothing back --
 * the two halves of "a value borrows its name" that a double free would fail
 * under the sanitizers. */
void test_symbols_copy_and_free_leave_the_name_alone() {
    const SummaSchemeValue source = summa_make_scheme_symbol(UNIQUE("borrow"));

    SummaSchemeValue copy = {};
    SUMMA_TEST_ASSERT(!summa_scheme_value_copy(&copy, &source).had);
    SUMMA_TEST_ASSERT_EQ(source.value.symbol.value, copy.value.symbol.value);

    summa_scheme_value_free(&copy);
    summa_scheme_value_free(&copy); /* Freeing twice has to stay a no-op. */

    /* The original still reads, because nothing was ever released. */
    SUMMA_TEST_ASSERT_EQ_STR(UNIQUE("borrow"), source.value.symbol.value->value);
    SUMMA_TEST_ASSERT_EQ(source.value.symbol.value, summa_scheme_symbol_intern(UNIQUE("borrow")));
}

/* `eqv?` on symbols is what interning is for, and summa_scheme_value_equals is
 * where that lands today. */
void test_symbols_equality_is_identity() {
    const SummaSchemeValue left  = summa_make_scheme_symbol(UNIQUE("eq"));
    const SummaSchemeValue right = summa_make_scheme_symbol(UNIQUE("eq"));
    const SummaSchemeValue other = summa_make_scheme_symbol(UNIQUE("neq"));

    SUMMA_TEST_ASSERT(summa_scheme_value_equals(&left, &right));
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &other));
}

/* A parameter bound in a frame and the name looked up in it are the same
 * record, which is what makes the lookup a pointer compare rather than a
 * strcmp. */
void test_symbols_a_bound_parameter_shares_its_name_with_the_procedure() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue result = {};
        const char*      cursor = "(define (identity parameter-name) parameter-name)";
        SUMMA_TEST_ASSERT(!summa_scheme_read(env, cursor, nullptr, &result).had);

        SummaSchemeValue definition = {};
        SUMMA_TEST_ASSERT(!summa_scheme_evaluate(env, result, &definition).had);
        summa_scheme_value_free(&result);
        summa_scheme_value_free(&definition);

        SummaSchemeBinding binding = {};
        SUMMA_TEST_ASSERT(
            !summa_scheme_environment_get_name(env, summa_scheme_symbol_intern("identity"), &binding).had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeProcedureType, binding.value.type);
        SUMMA_TEST_ASSERT_EQ(summa_scheme_symbol_intern("identity"), binding.value.value.procedure.name);
        SUMMA_TEST_ASSERT_EQ(1u, binding.value.value.procedure.bindings->length);
        SUMMA_TEST_ASSERT_EQ(summa_scheme_symbol_intern("parameter-name"),
                             binding.value.value.procedure.bindings->value[0].value);
    }
}

/* Calling a procedure a thousand times used to allocate a string per binding
 * per call. Now the whole run interns nothing at all after the source is
 * read. */
void test_symbols_calling_a_procedure_interns_nothing() {
    SCOPED_GLOBAL_ENV(env) {
        const char* cursor = "(define (add-three a-name b-name c-name) (+ a-name b-name c-name))"
                             "(define (spin n acc) (if (zero? n) acc (spin (- n 1) (add-three acc 1 0))))";
        while (*cursor) {
            SummaSchemeValue form = {};
            SUMMA_TEST_ASSERT(!summa_scheme_read(env, cursor, &cursor, &form).had);
            SummaSchemeValue ignored = {};
            SUMMA_TEST_ASSERT(!summa_scheme_evaluate(env, form, &ignored).had);
            summa_scheme_value_free(&form);
            summa_scheme_value_free(&ignored);
        }

        SummaSchemeValue call = {};
        SUMMA_TEST_ASSERT(!summa_scheme_read(env, "(spin 1000 0)", nullptr, &call).had);
        const size_t before = summa_scheme_symbol_interned_count();

        SummaSchemeValue result = {};
        SUMMA_TEST_ASSERT(!summa_scheme_evaluate(env, call, &result).had);

        SUMMA_TEST_ASSERT_EQ(1000, result.value.integer.value);
        SUMMA_TEST_ASSERT_EQ(before, summa_scheme_symbol_interned_count());

        summa_scheme_value_free(&call);
        summa_scheme_value_free(&result);
    }
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.symbols", argc, argv);
    SUMMA_TEST_RUN(test_symbols_the_same_name_interns_to_the_same_pointer);
    SUMMA_TEST_RUN(test_symbols_distinct_names_intern_to_distinct_pointers);
    SUMMA_TEST_RUN(test_symbols_reinterning_a_name_does_not_grow_the_table);
    SUMMA_TEST_RUN(test_symbols_a_growth_does_not_move_a_record);
    SUMMA_TEST_RUN(test_symbols_reading_the_same_source_twice_interns_once);
    SUMMA_TEST_RUN(test_symbols_copy_and_free_leave_the_name_alone);
    SUMMA_TEST_RUN(test_symbols_equality_is_identity);
    SUMMA_TEST_RUN(test_symbols_a_bound_parameter_shares_its_name_with_the_procedure);
    SUMMA_TEST_RUN(test_symbols_calling_a_procedure_interns_nothing);
    return summa_test_end();
}
