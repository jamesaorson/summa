#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <stdint.h>
#include <stdlib.h>

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

/* No reader yet, so programs are built as values. These keep the forms close
 * enough to Scheme to read. */
static SummaSchemeValue list_of(const SummaSchemeValue* values, size_t count) {
    SummaList list = summa_scheme_list_make_empty();
    for (size_t i = 0; i < count; i++) {
        summa_list_push(list, (SummaSchemeValue*)&values[i]);
    }
    return summa_make_scheme_list(list);
}

#define LIST(...)                                    \
    list_of((const SummaSchemeValue[]){__VA_ARGS__}, \
            sizeof((const SummaSchemeValue[]){__VA_ARGS__}) / sizeof(SummaSchemeValue))
#define EMPTY_LIST() summa_make_scheme_list(summa_scheme_list_make_empty())

#define SYM(name) summa_make_scheme_symbol(name)
#define INT(value) summa_make_scheme_integer(value)
#define STR(value) summa_make_scheme_string(value)
#define BOOL(value) summa_make_scheme_boolean(value)

/* The form is deep-owned, so one free reaches everything LIST() allocated. */
static SummaSchemeError eval(SummaSchemeEnvironment env, SummaSchemeValue in, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_evaluate(env, in, out);
    summa_scheme_value_free(&in);
    return err;
}

/* Setup forms, where only the effect on the environment matters. */
static void eval_ok(SummaSchemeEnvironment env, SummaSchemeValue in) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = eval(env, in, &out);
    SUMMA_TEST_ASSERT(!err.had);
    summa_scheme_value_free(&out);
}

static void assert_evaluates_to_integer(SummaSchemeEnvironment env, SummaSchemeValue in, int64_t expected) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = eval(env, in, &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, out.type);
    SUMMA_TEST_ASSERT_EQ(expected, out.value.integer.value);
    summa_scheme_value_free(&out);
}

static void assert_evaluation_fails(SummaSchemeEnvironment env, SummaSchemeValue in) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = eval(env, in, &out);
    SUMMA_TEST_ASSERT(err.had);
    if (!err.had) {
        summa_scheme_value_free(&out);
    }
}

/* Builds fine, always fails to evaluate: proves a branch was never taken. */
#define POISON() LIST(SYM("+"), INT(1), STR("not a number"))

#pragma region quote

void test_scheme_quote_leaves_its_operand_unevaluated() {
    SCOPED_GLOBAL_ENV(env) {
        /* (quote (+ 1 2)) is a three element list, not 3. */
        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("quote"), LIST(SYM("+"), INT(1), INT(2))), &out);

        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, out.type);
        SUMMA_TEST_ASSERT_EQ(3, out.value.list.value->length);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeSymbolType, out.value.list.value->value[0].type);
        summa_scheme_value_free(&out);
    }
}

/* Nothing about the operand is examined, not even enough to fail. */
void test_scheme_quote_accepts_a_form_that_would_not_evaluate() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("quote"), POISON()), &out);

        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, out.type);
        summa_scheme_value_free(&out);
    }
}

void test_scheme_quote_rejects_extra_operands() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluation_fails(env, LIST(SYM("quote"), INT(1), INT(2)));
    }
}

#pragma endregion quote

#pragma region if

void test_scheme_if_selects_the_consequent() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("if"), BOOL(true), INT(1), INT(2)), 1);
    }
}

void test_scheme_if_selects_the_alternate() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("if"), BOOL(false), INT(1), INT(2)), 2);
    }
}

/* If `if` were an ordinary procedure its operands would already be evaluated,
 * the poisoned branch would raise, and every recursive base case would
 * diverge. */
void test_scheme_if_does_not_evaluate_the_untaken_alternate() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("if"), BOOL(true), INT(1), POISON()), 1);
    }
}

void test_scheme_if_does_not_evaluate_the_untaken_consequent() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("if"), BOOL(false), POISON(), INT(2)), 2);
    }
}

/* Only #f is false: 0 and the empty list are both true. The empty list has to
 * be quoted, since `()` on its own is not an expression. */
void test_scheme_if_treats_every_non_false_value_as_true() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("if"), INT(0), INT(1), INT(2)), 1);
        assert_evaluates_to_integer(env, LIST(SYM("if"), LIST(SYM("quote"), EMPTY_LIST()), INT(1), INT(2)), 1);
    }
}

void test_scheme_if_without_an_alternate_falls_through() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("if"), BOOL(false), INT(1)), &out);

        SUMMA_TEST_ASSERT(!err.had);
        summa_scheme_value_free(&out);
    }
}

#pragma endregion if

#pragma region define and lambda

void test_scheme_define_binds_a_value() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(42)));
        assert_evaluates_to_integer(env, SYM("x"), 42);
    }
}

void test_scheme_define_evaluates_its_value_expression() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), LIST(SYM("+"), INT(1), INT(2))));
        assert_evaluates_to_integer(env, SYM("x"), 3);
    }
}

void test_scheme_define_rebinds_an_existing_name() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(1)));
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(2)));
        assert_evaluates_to_integer(env, SYM("x"), 2);
    }
}

void test_scheme_define_procedure_shorthand_is_callable() {
    SCOPED_GLOBAL_ENV(env) {
        /* (define (add2 x y) (+ x y)) */
        eval_ok(env, LIST(SYM("define"), LIST(SYM("add2"), SYM("x"), SYM("y")), LIST(SYM("+"), SYM("x"), SYM("y"))));
        assert_evaluates_to_integer(env, LIST(SYM("add2"), INT(20), INT(22)), 42);
    }
}

void test_scheme_lambda_is_applied_in_operator_position() {
    SCOPED_GLOBAL_ENV(env) {
        /* ((lambda (x) (+ x 1)) 41) -- the operator is an expression, not a name. */
        assert_evaluates_to_integer(
            env, LIST(LIST(SYM("lambda"), LIST(SYM("x")), LIST(SYM("+"), SYM("x"), INT(1))), INT(41)), 42);
    }
}

void test_scheme_lambda_bound_by_define_is_callable() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(
            env,
            LIST(SYM("define"), SYM("inc"), LIST(SYM("lambda"), LIST(SYM("x")), LIST(SYM("+"), SYM("x"), INT(1)))));
        assert_evaluates_to_integer(env, LIST(SYM("inc"), INT(41)), 42);
    }
}

void test_scheme_procedure_arity_is_checked() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), LIST(SYM("add2"), SYM("x"), SYM("y")), LIST(SYM("+"), SYM("x"), SYM("y"))));
        assert_evaluation_fails(env, LIST(SYM("add2"), INT(1)));
        assert_evaluation_fails(env, LIST(SYM("add2"), INT(1), INT(2), INT(3)));
    }
}

void test_scheme_lambda_requires_a_body() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluation_fails(env, LIST(SYM("lambda"), LIST(SYM("x"))));
    }
}

void test_scheme_lambda_rejects_non_symbol_parameters() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluation_fails(env, LIST(SYM("lambda"), LIST(INT(1)), INT(2)));
    }
}

/* A body sees where it was defined, not where it was called -- the reason a
 * procedure carries a closure handle at all. */
void test_scheme_procedures_are_lexically_scoped() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(10)));
        eval_ok(env, LIST(SYM("define"), LIST(SYM("get")), SYM("x")));

        /* Under dynamic scoping the inner binding would win and this would
         * be 20. */
        assert_evaluates_to_integer(env, LIST(SYM("let"), LIST(LIST(SYM("x"), INT(20))), LIST(SYM("get"))), 10);
    }
}

/* Shadowed for the call, intact afterwards. */
void test_scheme_parameters_shadow_outer_bindings() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(1)));
        eval_ok(env, LIST(SYM("define"), LIST(SYM("identity"), SYM("x")), SYM("x")));

        assert_evaluates_to_integer(env, LIST(SYM("identity"), INT(99)), 99);
        assert_evaluates_to_integer(env, SYM("x"), 1);
    }
}

#pragma endregion define and lambda

#pragma region recursion

/* The base case is reached because `if` left the recursive branch alone. The
 * stop flag stands in for the comparison builtin that does not exist yet. */
void test_scheme_recursion_terminates() {
    SCOPED_GLOBAL_ENV(env) {
        /* (define (walk stop n) (if stop n (walk #t (+ n 1)))) */
        eval_ok(env,
                LIST(SYM("define"),
                     LIST(SYM("walk"), SYM("stop"), SYM("n")),
                     LIST(SYM("if"),
                          SYM("stop"),
                          SYM("n"),
                          LIST(SYM("walk"), BOOL(true), LIST(SYM("+"), SYM("n"), INT(1))))));

        assert_evaluates_to_integer(env, LIST(SYM("walk"), BOOL(true), INT(7)), 7);
        assert_evaluates_to_integer(env, LIST(SYM("walk"), BOOL(false), INT(7)), 8);
    }
}

/* The depth guard, which survives proper tail calls rather than being replaced
 * by them. `(+ 1 (loop))` recurses in an *operand*, so every level has work
 * left to do and genuinely needs a C frame; SUMMA_SCHEME_MAX_DEPTH is what
 * turns that into a diagnosable error instead of a segfault.
 *
 * Deliberately not `(define (loop) (loop))`, which the trampoline runs as the
 * correct non-terminating program it is -- a case that would hang rather than
 * fail. tests/scheme/scheme.tail_calls.test.c covers that side. */
void test_scheme_runaway_non_tail_recursion_is_an_error_not_a_crash() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), LIST(SYM("loop")), LIST(SYM("+"), INT(1), LIST(SYM("loop")))));
        assert_evaluation_fails(env, LIST(SYM("loop")));
    }
}

#pragma endregion recursion

#pragma region set! and begin

void test_scheme_set_mutates_an_existing_binding() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(1)));
        eval_ok(env, LIST(SYM("set!"), SYM("x"), INT(2)));
        assert_evaluates_to_integer(env, SYM("x"), 2);
    }
}

/* Reaching an outer binding from an inner frame is what distinguishes set!
 * from define, which would shadow instead. */
void test_scheme_set_reaches_an_enclosing_binding() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("counter"), INT(0)));
        eval_ok(env, LIST(SYM("define"), LIST(SYM("bump")), LIST(SYM("set!"), SYM("counter"), INT(5))));
        eval_ok(env, LIST(SYM("bump")));

        assert_evaluates_to_integer(env, SYM("counter"), 5);
    }
}

void test_scheme_set_rejects_an_unbound_name() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluation_fails(env, LIST(SYM("set!"), SYM("never-defined"), INT(1)));
    }
}

void test_scheme_begin_yields_its_last_expression() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("begin"), INT(1), INT(2), INT(3)), 3);
    }
}

void test_scheme_begin_evaluates_every_expression() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(0)));
        assert_evaluates_to_integer(env, LIST(SYM("begin"), LIST(SYM("set!"), SYM("x"), INT(9)), INT(1)), 1);
        assert_evaluates_to_integer(env, SYM("x"), 9);
    }
}

#pragma endregion set !and begin

#pragma region let

void test_scheme_let_binds_for_its_body() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(
            env,
            LIST(SYM("let"), LIST(LIST(SYM("x"), INT(1)), LIST(SYM("y"), INT(2))), LIST(SYM("+"), SYM("x"), SYM("y"))),
            3);
    }
}

void test_scheme_let_bindings_do_not_outlive_the_body() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("let"), LIST(LIST(SYM("scoped"), INT(1))), SYM("scoped")));

        /* The name is gone with the frame, so referencing it afterwards is an
         * unbound variable. */
        assert_evaluation_fails(env, SYM("scoped"));
    }
}

/* Simultaneous: `x` here is the outer 1, not the 100 bound alongside. */
void test_scheme_let_initializers_do_not_see_their_siblings() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(1)));
        assert_evaluates_to_integer(
            env, LIST(SYM("let"), LIST(LIST(SYM("x"), INT(100)), LIST(SYM("y"), SYM("x"))), SYM("y")), 1);
    }
}

/* Same shape, opposite rule: each initializer sees the ones before it. */
void test_scheme_let_star_initializers_see_the_previous_ones() {
    SCOPED_GLOBAL_ENV(env) {
        eval_ok(env, LIST(SYM("define"), SYM("x"), INT(1)));
        assert_evaluates_to_integer(
            env, LIST(SYM("let*"), LIST(LIST(SYM("x"), INT(100)), LIST(SYM("y"), SYM("x"))), SYM("y")), 100);
    }
}

/* Every name bound before any initializer runs, so one procedure can name
 * another defined beside it. */
void test_scheme_letrec_allows_a_body_to_reference_a_sibling() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(
            env,
            LIST(SYM("letrec"),
                 LIST(LIST(SYM("one"), LIST(SYM("lambda"), EMPTY_LIST(), INT(1))),
                      LIST(SYM("two"), LIST(SYM("lambda"), EMPTY_LIST(), LIST(SYM("+"), LIST(SYM("one")), INT(1))))),
                 LIST(SYM("two"))),
            2);
    }
}

void test_scheme_let_rejects_a_malformed_binding() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluation_fails(env, LIST(SYM("let"), LIST(LIST(SYM("x"))), SYM("x")));
        assert_evaluation_fails(env, LIST(SYM("let"), LIST(INT(1)), INT(2)));
    }
}

#pragma endregion let

#pragma region cond, and, or, when, unless

void test_scheme_cond_takes_the_first_true_clause() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(
            env, LIST(SYM("cond"), LIST(BOOL(false), INT(1)), LIST(BOOL(true), INT(2)), LIST(BOOL(true), INT(3))), 2);
    }
}

void test_scheme_cond_falls_through_to_else() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("cond"), LIST(BOOL(false), INT(1)), LIST(SYM("else"), INT(2))), 2);
    }
}

/* Neither the failed clauses' bodies nor anything after the match. */
void test_scheme_cond_evaluates_no_other_clause_body() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(
            env,
            LIST(SYM("cond"), LIST(BOOL(false), POISON()), LIST(BOOL(true), INT(2)), LIST(BOOL(true), POISON())),
            2);
    }
}

/* A clause that is only a test yields the test's own value. */
void test_scheme_cond_clause_without_a_body_yields_its_test() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("cond"), LIST(LIST(SYM("+"), INT(2), INT(3)))), 5);
    }
}

void test_scheme_cond_without_a_match_falls_through() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("cond"), LIST(BOOL(false), INT(1))), &out);
        SUMMA_TEST_ASSERT(!err.had);
        summa_scheme_value_free(&out);
    }
}

/* `and` yields the operand that decided it, not a boolean. */
void test_scheme_and_yields_its_last_operand() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("and"), INT(1), INT(2), INT(3)), 3);
    }
}

void test_scheme_and_short_circuits_on_false() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("and"), INT(1), BOOL(false), POISON()), &out);

        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeBooleanType, out.type);
        SUMMA_TEST_ASSERT_EQ(false, out.value.boolean.value);
        summa_scheme_value_free(&out);
    }
}

void test_scheme_and_with_no_operands_is_true() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("and")), &out);
        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeBooleanType, out.type);
        SUMMA_TEST_ASSERT_EQ(true, out.value.boolean.value);
        summa_scheme_value_free(&out);
    }
}

void test_scheme_or_yields_its_first_true_operand() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("or"), BOOL(false), INT(2), INT(3)), 2);
    }
}

void test_scheme_or_short_circuits_on_true() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("or"), INT(1), POISON()), 1);
    }
}

void test_scheme_or_with_no_operands_is_false() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("or")), &out);
        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeBooleanType, out.type);
        SUMMA_TEST_ASSERT_EQ(false, out.value.boolean.value);
        summa_scheme_value_free(&out);
    }
}

void test_scheme_when_runs_its_body_only_when_true() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("when"), BOOL(true), INT(1), INT(2)), 2);

        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("when"), BOOL(false), POISON()), &out);
        SUMMA_TEST_ASSERT(!err.had);
        summa_scheme_value_free(&out);
    }
}

void test_scheme_unless_runs_its_body_only_when_false() {
    SCOPED_GLOBAL_ENV(env) {
        assert_evaluates_to_integer(env, LIST(SYM("unless"), BOOL(false), INT(1), INT(2)), 2);

        SummaSchemeValue       out = {};
        const SummaSchemeError err = eval(env, LIST(SYM("unless"), BOOL(true), POISON()), &out);
        SUMMA_TEST_ASSERT(!err.had);
        summa_scheme_value_free(&out);
    }
}

#pragma endregion cond, and, or, when, unless

int main(int argc, char** argv) {
    summa_test_begin("scheme.special_forms", argc, argv);

    SUMMA_TEST_RUN(test_scheme_quote_leaves_its_operand_unevaluated);
    SUMMA_TEST_RUN(test_scheme_quote_accepts_a_form_that_would_not_evaluate);
    SUMMA_TEST_RUN(test_scheme_quote_rejects_extra_operands);

    SUMMA_TEST_RUN(test_scheme_if_selects_the_consequent);
    SUMMA_TEST_RUN(test_scheme_if_selects_the_alternate);
    SUMMA_TEST_RUN(test_scheme_if_does_not_evaluate_the_untaken_alternate);
    SUMMA_TEST_RUN(test_scheme_if_does_not_evaluate_the_untaken_consequent);
    SUMMA_TEST_RUN(test_scheme_if_treats_every_non_false_value_as_true);
    SUMMA_TEST_RUN(test_scheme_if_without_an_alternate_falls_through);

    SUMMA_TEST_RUN(test_scheme_define_binds_a_value);
    SUMMA_TEST_RUN(test_scheme_define_evaluates_its_value_expression);
    SUMMA_TEST_RUN(test_scheme_define_rebinds_an_existing_name);
    SUMMA_TEST_RUN(test_scheme_define_procedure_shorthand_is_callable);
    SUMMA_TEST_RUN(test_scheme_lambda_is_applied_in_operator_position);
    SUMMA_TEST_RUN(test_scheme_lambda_bound_by_define_is_callable);
    SUMMA_TEST_RUN(test_scheme_procedure_arity_is_checked);
    SUMMA_TEST_RUN(test_scheme_lambda_requires_a_body);
    SUMMA_TEST_RUN(test_scheme_lambda_rejects_non_symbol_parameters);
    SUMMA_TEST_RUN(test_scheme_procedures_are_lexically_scoped);
    SUMMA_TEST_RUN(test_scheme_parameters_shadow_outer_bindings);

    SUMMA_TEST_RUN(test_scheme_recursion_terminates);
    SUMMA_TEST_RUN(test_scheme_runaway_non_tail_recursion_is_an_error_not_a_crash);

    SUMMA_TEST_RUN(test_scheme_set_mutates_an_existing_binding);
    SUMMA_TEST_RUN(test_scheme_set_reaches_an_enclosing_binding);
    SUMMA_TEST_RUN(test_scheme_set_rejects_an_unbound_name);
    SUMMA_TEST_RUN(test_scheme_begin_yields_its_last_expression);
    SUMMA_TEST_RUN(test_scheme_begin_evaluates_every_expression);

    SUMMA_TEST_RUN(test_scheme_let_binds_for_its_body);
    SUMMA_TEST_RUN(test_scheme_let_bindings_do_not_outlive_the_body);
    SUMMA_TEST_RUN(test_scheme_let_initializers_do_not_see_their_siblings);
    SUMMA_TEST_RUN(test_scheme_let_star_initializers_see_the_previous_ones);
    SUMMA_TEST_RUN(test_scheme_letrec_allows_a_body_to_reference_a_sibling);
    SUMMA_TEST_RUN(test_scheme_let_rejects_a_malformed_binding);

    SUMMA_TEST_RUN(test_scheme_cond_takes_the_first_true_clause);
    SUMMA_TEST_RUN(test_scheme_cond_falls_through_to_else);
    SUMMA_TEST_RUN(test_scheme_cond_evaluates_no_other_clause_body);
    SUMMA_TEST_RUN(test_scheme_cond_clause_without_a_body_yields_its_test);
    SUMMA_TEST_RUN(test_scheme_cond_without_a_match_falls_through);
    SUMMA_TEST_RUN(test_scheme_and_yields_its_last_operand);
    SUMMA_TEST_RUN(test_scheme_and_short_circuits_on_false);
    SUMMA_TEST_RUN(test_scheme_and_with_no_operands_is_true);
    SUMMA_TEST_RUN(test_scheme_or_yields_its_first_true_operand);
    SUMMA_TEST_RUN(test_scheme_or_short_circuits_on_true);
    SUMMA_TEST_RUN(test_scheme_or_with_no_operands_is_false);
    SUMMA_TEST_RUN(test_scheme_when_runs_its_body_only_when_true);
    SUMMA_TEST_RUN(test_scheme_unless_runs_its_body_only_when_false);

    return summa_test_end();
}
