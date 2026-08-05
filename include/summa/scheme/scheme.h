#ifndef SUMMA_SCHEME_H
#define SUMMA_SCHEME_H

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define SUMMA_ARRAY_IMPLEMENTATION
#include <summa/array/array.h>
#define SUMMA_HASH_SET_IMPLEMENTATION
#include <summa/hash_set/hash_set.h>
#define SUMMA_REF_COUNT_IMPLEMENTATION
#include <summa/ref_count/ref_count.h>
#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#pragma region Error handling

typedef struct {
    bool        had;
    const char* message;
} SummaSchemeError;

#define summa_success() ((SummaSchemeError){.had = false})

#define summa_make_error(msg) \
    ((SummaSchemeError){      \
        .had     = true,      \
        .message = (msg),     \
    })

#pragma endregion Error handling

#pragma region Values

typedef struct SummaSchemeValue SummaSchemeValue;

/* Procedures capture their defining environment, so the handle is needed below. */
typedef struct SummaSchemeEnvironment_t* SummaSchemeEnvironment;

SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaList, list, SummaSchemeValue)

SummaSchemeError summa_scheme_value_copy(SummaSchemeValue* dest, const SummaSchemeValue* src);
bool             summa_scheme_value_equals(const SummaSchemeValue* left, const SummaSchemeValue* right);

/* Mirrors summa_scheme_value_copy: both are deep, so a copied container owns
 * its elements and can outlive whatever built it. Builtins depend on that --
 * arguments are released as soon as dispatch returns.
 *
 * A procedure's captured environment is borrowed, and not freed here. */
void summa_scheme_value_free(SummaSchemeValue* value);

typedef enum {
    SummaSchemeBooleanType,
    SummaSchemeCharacterType,
    SummaSchemeFloatingType,
    SummaSchemeIntegerType,
    SummaSchemeListType,
    SummaSchemeProcedureType,
    SummaSchemeStringType,
    SummaSchemeSymbolType,
    SummaSchemeVectorType,
} SummaSchemeValueType;

typedef struct {
    bool value;
} SummaSchemeBoolean;

#define summa_make_scheme_boolean(val) \
    ((SummaSchemeValue){.type = SummaSchemeBooleanType, .value.boolean = {.value = (val)}})

#define SUMMA_SCHEME_TRUE "#t"
#define SUMMA_SCHEME_FALSE "#f"

typedef struct {
    char value;
} SummaSchemeCharacter;

#define summa_make_scheme_character(val) \
    ((SummaSchemeValue){.type = SummaSchemeCharacterType, .value.character = {.value = (val)}})

typedef struct {
    double value;
} SummaSchemeFloating;

#define summa_make_scheme_floating(val) \
    ((SummaSchemeValue){.type = SummaSchemeFloatingType, .value.floating = {.value = (val)}})

typedef struct {
    int64_t value;
} SummaSchemeInteger;

#define summa_make_scheme_integer(val) \
    ((SummaSchemeValue){.type = SummaSchemeIntegerType, .value.integer = {.value = (val)}})

/* ── Interned names ────────────────────────────────────────────────────────
 *
 * A name -- a symbol, a binding, a procedure's own name -- is one canonical
 * record for the life of the process, and every value that uses it borrows a
 * pointer to that record. Two consequences, and both are the point:
 *
 * - **A name is compared by identity.** `left == right` decides two symbols,
 *   where a `SummaString` needed `strcmp` down every frame in the chain.
 * - **A name is never allocated twice.** Binding a parameter used to allocate a
 *   fresh string for a name that already existed in the source; now it copies a
 *   pointer.
 *
 * Nothing owns a name, so nothing frees one: `summa_scheme_value_copy` copies
 * the pointer and `summa_scheme_value_free` has nothing to give back. That is
 * the one rule to keep in mind when adding a value type that carries a name.
 *
 * The record's characters live in the same allocation, so `name->value` reads
 * as a C string exactly as `SummaString::value` did, and a name costs one
 * malloc rather than the two a `SummaString` handle plus buffer cost. */
typedef struct SummaSchemeSymbolName_t SummaSchemeSymbolName_t;

struct SummaSchemeSymbolName_t {
    /* The next name in this bucket. A growth rewires these; a record's own
     * address never moves, which is what makes an interned pointer safe to
     * hold for as long as the process lives. */
    SummaSchemeSymbolName_t* next;
    /* djb2 over the characters, computed once at intern time. A hash-based
     * binding lookup has nothing left to recompute. */
    SummaHashCode hash;
    size_t        length;
    char          value[];
};

typedef const SummaSchemeSymbolName_t* SummaSchemeSymbolName;

/* The canonical record for `name`, making it if this is the first time it has
 * been seen. Interning the same characters twice hands back the same pointer
 * and allocates nothing the second time. */
SummaSchemeSymbolName summa_scheme_symbol_intern(const char* name);

/* Distinct names interned so far. The table only ever grows, so a test pins
 * "interning twice allocates once" by reading this either side of the second
 * intern rather than by counting mallocs. */
size_t summa_scheme_symbol_interned_count(void);

typedef struct {
    SummaSchemeSymbolName value;
} SummaSchemeSymbol;

#define summa_make_scheme_symbol(val) \
    ((SummaSchemeValue){.type = SummaSchemeSymbolType, .value.symbol = {.value = summa_scheme_symbol_intern(val)}})

/* The same, for a name already interned -- which by the time the evaluator is
 * running is every name it has. */
#define summa_make_scheme_symbol_name(name_) \
    ((SummaSchemeValue){.type = SummaSchemeSymbolType, .value.symbol = {.value = (name_)}})

SUMMA_ARRAY_GENERATE_TYPE(SummaSchemeSymbolList, symbol_list, SummaSchemeSymbol)

/* A null `body` marks a builtin; dispatch sends it to the procedure table.
 *
 * `closure` is the defining environment, retained like
 * SummaSchemeEnvironment_t::parent: a procedure holds the frame it was defined
 * in up for as long as the procedure itself lives, which is what lets a closure
 * outlive the call that made it. summa_scheme_value_copy acquires it and
 * summa_scheme_value_free releases it -- see BUILTINS.md. */
typedef struct {
    SummaSchemeSymbolName  name;
    SummaSchemeSymbolList  bindings;
    SummaList              body;
    SummaSchemeEnvironment closure;
} SummaSchemeProcedure;

#define summa_make_scheme_procedure(name_, bindings_, body_)         \
    ((SummaSchemeValue){.type            = SummaSchemeProcedureType, \
                        .value.procedure = {                         \
                            .name     = (name_),                     \
                            .bindings = (bindings_),                 \
                            .body     = (body_),                     \
                            .closure  = nullptr,                     \
                        }})

#define summa_make_scheme_closure(name_, bindings_, body_, closure_) \
    ((SummaSchemeValue){.type            = SummaSchemeProcedureType, \
                        .value.procedure = {                         \
                            .name     = (name_),                     \
                            .bindings = (bindings_),                 \
                            .body     = (body_),                     \
                            .closure  = (closure_),                  \
                        }})

typedef struct {
    SummaString value;
} SummaSchemeString;

#define summa_make_scheme_string(val) \
    ((SummaSchemeValue){.type = SummaSchemeStringType, .value.string = {.value = summa_string_make(val)}})

typedef struct {
    SummaList value;
} SummaSchemeList;

#define summa_make_scheme_list(val) ((SummaSchemeValue){.type = SummaSchemeListType, .value.list = {.value = (val)}})

typedef struct {
    SummaList value;
} SummaSchemeVector;

#define summa_make_scheme_vector(val) \
    ((SummaSchemeValue){.type = SummaSchemeVectorType, .value.vector = {.value = (val)}})

typedef union {
    SummaSchemeBoolean   boolean;
    SummaSchemeCharacter character;
    SummaSchemeFloating  floating;
    SummaSchemeInteger   integer;
    SummaSchemeList      list;
    SummaSchemeProcedure procedure;
    SummaSchemeString    string;
    SummaSchemeSymbol    symbol;
    SummaSchemeValueType type;
    SummaSchemeVector    vector;
} SummaSchemeValueUnion;

struct SummaSchemeValue {
    SummaSchemeValueType  type;
    SummaSchemeValueUnion value;
};

SUMMA_ARRAY_GENERATE_TYPE_IMPL(SummaList, list, SummaSchemeValue)

/* `name` is interned and therefore borrowed: the environment owns the value and
 * nothing else. */
typedef struct {
    SummaSchemeSymbolName name;
    SummaSchemeValue      value;
} SummaSchemeBinding;

#define summa_scheme_binding_make(name_, value_) \
    (SummaSchemeBinding) {                       \
        .name = name_, .value = value_           \
    }

SUMMA_ARRAY_GENERATE_TYPE(SummaSchemeBindingList, binding_list, SummaSchemeBinding)

/* The trailing four fields are the cycle collector's, not the evaluator's: the
 * registry links every live environment into one intrusive list, and the two
 * scratch fields are written and read within a single collection pass. See
 * "Cycle collection" in the implementation section. */
struct SummaSchemeEnvironment_t {
    SummaSchemeBindingList bindings;
    SummaSchemeEnvironment parent;

    SummaSchemeEnvironment registry_previous;
    SummaSchemeEnvironment registry_next;
    size_t                 trial_count;
    bool                   reachable;
};

/* Environments are heap-allocated and reference counted, so they outlive the
 * block that built them exactly as long as something still points at them --
 * which is what lets a procedure capture its defining environment.
 *
 * The count lives in a `RefCount` header carved out of the same allocation, so
 * the handle a caller holds is the payload and `env - 1` is its counter. `make`
 * hands back the only reference, at a count of one.
 *
 * `parent` is retained, not borrowed: a child holds its parent up, so freeing a
 * parent while a child still points at it is safe rather than a dangling
 * chain. */
SummaSchemeEnvironment summa_scheme_environment_make(SummaSchemeEnvironment parent);
SummaSchemeEnvironment summa_scheme_environment_make_global(void);

/* The same, with room for exactly `count` bindings.
 *
 * `make` is this with a count of zero, and zero allocates no binding storage at
 * all. That is deliberate: whoever makes an environment almost always knows how
 * many bindings are about to go in it -- a call knows its arity, a `let` knows
 * its clause count -- and the old default guess of eight was wrong for nearly
 * every frame. A frame that binds more later, a body with an internal `define`,
 * grows the ordinary way from whatever it was given. */
SummaSchemeEnvironment summa_scheme_environment_make_with_capacity(SummaSchemeEnvironment parent, size_t count);

/* Grows an existing environment's binding storage to hold at least `count`, and
 * never shrinks it. For a frame whose size is learned after it is made. */
void summa_scheme_environment_reserve(SummaSchemeEnvironment env, size_t count);

/* One more reference, and one fewer. `acquire` returns its argument so a slot
 * can be filled and retained in one expression; both take null and do nothing
 * with it.
 *
 * The last release runs the environment's teardown: its parent and every
 * environment its bindings reach are released in turn, then the binding list
 * and every name and value in it are freed. */
SummaSchemeEnvironment summa_scheme_environment_acquire(const SummaSchemeEnvironment env);
void                   summa_scheme_environment_release(const SummaSchemeEnvironment env);

/* Drops a reference the way `release` does, and then collects cycles.
 *
 * This is the caller-facing end of an environment's life -- the call that
 * matches `make_global` at the end of a program -- rather than the one the
 * evaluator uses per frame. A program's last act leaves nothing behind even if
 * it never made enough environments to reach the automatic threshold, which is
 * what makes `live_count` an honest reading once a run is over. */
void summa_scheme_environment_free(const SummaSchemeEnvironment env);

/* Environments made but not yet freed -- the length of the registry the
 * collector walks. An evaluation that has run to completion and had its global
 * environment torn down leaves this back at whatever it was beforehand;
 * anything else is an environment nobody can reach and nobody will free.
 *
 * That is not a hypothetical now that environments are reference counted: a
 * procedure captures the environment it was defined in, so a `letrec` or an
 * internal `define` binding a procedure into the very frame it closes over
 * makes a cycle, and a cycle's count never reaches zero. This is the instrument
 * those leaks are visible through. Single-threaded, like SUMMA_SCHEME_DEPTH. */
size_t summa_scheme_environment_live_count(void);

/* Runs a trial-deletion pass over every live environment and returns how many
 * it reclaimed. Collection is automatic -- see SUMMA_SCHEME_GC_MIN_THRESHOLD --
 * so this is for a host that wants a pass at a moment of its own choosing, and
 * for tests that want to observe one. */
size_t summa_scheme_collect_cycles(void);

void summa_scheme_environment_init_global(SummaSchemeEnvironment env);

#define summa_scheme_environment_make_empty() summa_scheme_environment_make(nullptr)

/* Binds `newBinding` in `env`, replacing any binding of the same name. The
 * environment takes ownership of the binding's *value* -- the name is interned
 * and belongs to nobody -- so the caller must not free the value after the
 * call, on either path. */
SummaSchemeError summa_scheme_environment_set(const SummaSchemeEnvironment env, SummaSchemeBinding newBinding);

SummaSchemeError summa_scheme_environment_get(const SummaSchemeEnvironment env,
                                              const SummaSchemeSymbol      symbol,
                                              SummaSchemeBinding*          out);

SummaSchemeError summa_scheme_environment_get_name(const SummaSchemeEnvironment env,
                                                   const SummaSchemeSymbolName  name,
                                                   SummaSchemeBinding*          out);

/* The same lookup, and also reports *which* environment in the chain held the
 * binding. `*out_owner` is borrowed -- it is an ancestor of `env`, so it lives
 * at least as long as `env` does, and outliving `env` takes an acquire.
 *
 * That is what the tail-call trampoline needs it for. A tail call releases the
 * frame it is leaving, and the procedure it is calling may be bound in that
 * very frame -- `(define (twice f x) (f (f x)))`, where `f` is a parameter --
 * so the frame's own binding owns the body about to run. Retaining the holder
 * for the duration of the call is the cheap fix; deep-copying the body per
 * iteration is the expensive one. */
SummaSchemeError summa_scheme_environment_get_owner(const SummaSchemeEnvironment env,
                                                    const SummaSchemeSymbolName  name,
                                                    SummaSchemeBinding*          out,
                                                    SummaSchemeEnvironment*      out_owner);

/* Rebinds a name wherever the chain already holds it, which environment_set
 * deliberately does not do. Takes ownership of `value` only on success; an
 * unbound name is an error and leaves it with the caller. */
SummaSchemeError summa_scheme_environment_assign(const SummaSchemeEnvironment env,
                                                 const SummaSchemeSymbolName  name,
                                                 SummaSchemeValue             value);

/* #f alone is false. Zero and the empty list are both true. */
bool summa_scheme_truthy(const SummaSchemeValue* value);

#pragma endregion Values

#pragma region REPL

/* Reads one datum from `input`.
 *
 * `rest`, when not null, is set past the datum and past any whitespace or
 * comment following it, so it lands either on the next datum's first character
 * or on the terminating NUL. That makes the driver loop `while (*cursor)`:
 *
 *     const char* cursor = line;
 *     while (*cursor) {
 *         err = summa_scheme_read(env, cursor, &cursor, &value);
 *     }
 *
 * `*rest` is untouched on error. Input holding no datum at all is an error, so
 * the loop above never calls in with nothing left to read. */
SummaSchemeError
summa_scheme_read(const SummaSchemeEnvironment env, const char* input, const char** rest, SummaSchemeValue* out);

/* Translates one R7RS string escape (7.1.1). `input` points at the backslash;
 * `*out` receives the single byte the sequence denotes and `rest`, when not
 * null, is set just past the sequence. Both are untouched on error.
 *
 * `\a \b \t \n \r` are the named control characters; `\" \\ \|` stand for
 * themselves; `\x<hex>;` is a byte in hex. Everything else -- including the
 * line-continuation escape, which spans a newline and yields no byte at all --
 * is an error. */
SummaSchemeError summa_scheme_read_escape(const char* input, const char** rest, char* out);

SummaSchemeError
summa_scheme_evaluate(const SummaSchemeEnvironment env, const SummaSchemeValue in, SummaSchemeValue* out);

/* `print` is R7RS `write`: output reads back as source, so strings are quoted
 * and characters carry their `#\` prefix. `display` is the same traversal
 * rendered for a human -- strings raw and characters as themselves, nested ones
 * included. */
SummaSchemeError summa_scheme_print(const SummaSchemeValue value, FILE* out);
SummaSchemeError summa_scheme_display(const SummaSchemeValue value, FILE* out);

/* The ceiling on *non-tail* recursion -- an error beats walking off the C
 * stack. Tail calls cost no depth at all: the trampoline in
 * `summa_scheme_evaluate_inner` loops instead of recursing, so a tail loop runs
 * as long as its own arithmetic says it should. Non-tail recursion genuinely
 * needs a C frame per level, which is what this bounds. */
#define SUMMA_SCHEME_MAX_DEPTH 2000

/* The same guard for reading, which recurses once per level of nesting. Far
 * lower than the evaluate limit because source that nests this deep is a
 * runaway `(((((...`, not a program someone wrote. */
#define SUMMA_SCHEME_READ_MAX_DEPTH 200

#pragma endregion REPL

#endif

#ifdef SUMMA_SCHEME_IMPLEMENTATION

#define ERROR_MESSAGE_LENGTH 1024
char ERROR_MESSAGE[ERROR_MESSAGE_LENGTH];

#pragma region Interned names

/* ── The intern table ──────────────────────────────────────────────────────
 *
 * Chained buckets, doubling once the population passes the bucket count. A
 * record is allocated once and never moves -- a growth rewires `next` and
 * nothing else -- so a pointer handed out by an earlier intern stays good
 * across every later one. That is the whole reason a value can hold a bare
 * pointer to a name.
 *
 * There is no way to empty this table, and that is deliberate rather than an
 * omission. Every symbol in every live value points into it, so releasing it
 * would invalidate all of them at once, and a table that can be emptied invites
 * exactly that call. It is the bargain a process already makes with its string
 * literals, and its size is bounded by the distinct identifiers a program
 * *mentions* rather than by how long the program runs: the euler suites reach a
 * few dozen names and stay there through 10^8 calls.
 *
 * `summa_hash` is the one summa already has -- djb2, from
 * `summa/hash_set/hash_set.h`. `SummaHashMap` was the other candidate for
 * storage and is not usable here: it keys on the hash code alone, so two names
 * that collide would be the same entry, and its key must be a fixed size that a
 * name does not have.
 *
 * Single-threaded, like SUMMA_SCHEME_ENVIRONMENT_REGISTRY and
 * SUMMA_SCHEME_DEPTH. */
#define SUMMA_SCHEME_SYMBOL_MIN_BUCKETS 64

static SummaSchemeSymbolName_t** SUMMA_SCHEME_SYMBOL_BUCKETS      = nullptr;
static size_t                    SUMMA_SCHEME_SYMBOL_BUCKET_COUNT = 0;
static size_t                    SUMMA_SCHEME_SYMBOL_COUNT        = 0;

size_t summa_scheme_symbol_interned_count(void) {
    return SUMMA_SCHEME_SYMBOL_COUNT;
}

/* Interns the names the evaluator itself compares against -- the special forms,
 * the builtins, `else`, `lambda` -- so those comparisons are pointer equality
 * too. Defined once both tables are in scope; called from
 * summa_scheme_symbol_intern, which is the choke point every name goes through,
 * so anything holding a SummaSchemeSymbolName is running after it. */
static void summa_scheme_symbols_ensure(void);

/* The two names the evaluator compares against outside of a table: `lambda`,
 * which every anonymous procedure is called, and `else`, which `cond` looks for
 * at the head of a clause. */
static SummaSchemeSymbolName SUMMA_SCHEME_SYMBOL_LAMBDA = nullptr;
static SummaSchemeSymbolName SUMMA_SCHEME_SYMBOL_ELSE   = nullptr;

/* Power of two, so the bucket index is a mask rather than a modulo. */
static void summa_scheme_symbol_table_grow(void) {
    const size_t next_count =
        SUMMA_SCHEME_SYMBOL_BUCKET_COUNT ? SUMMA_SCHEME_SYMBOL_BUCKET_COUNT * 2 : SUMMA_SCHEME_SYMBOL_MIN_BUCKETS;
    SummaSchemeSymbolName_t** const next_buckets = calloc(next_count, sizeof(SummaSchemeSymbolName_t*));
    assert(next_buckets);

    for (size_t i = 0; i < SUMMA_SCHEME_SYMBOL_BUCKET_COUNT; i++) {
        SummaSchemeSymbolName_t* entry = SUMMA_SCHEME_SYMBOL_BUCKETS[i];
        while (entry) {
            SummaSchemeSymbolName_t* const following = entry->next;
            const size_t                   index     = (size_t)entry->hash & (next_count - 1);
            entry->next                              = next_buckets[index];
            next_buckets[index]                      = entry;
            entry                                    = following;
        }
    }

    free(SUMMA_SCHEME_SYMBOL_BUCKETS);
    SUMMA_SCHEME_SYMBOL_BUCKETS      = next_buckets;
    SUMMA_SCHEME_SYMBOL_BUCKET_COUNT = next_count;
}

SummaSchemeSymbolName summa_scheme_symbol_intern(const char* name) {
    summa_scheme_symbols_ensure();

    if (SUMMA_SCHEME_SYMBOL_BUCKET_COUNT == 0) {
        summa_scheme_symbol_table_grow();
    }

    const size_t        length = strlen(name);
    const SummaHashCode hash   = summa_hash(name, length);
    const size_t        index  = (size_t)hash & (SUMMA_SCHEME_SYMBOL_BUCKET_COUNT - 1);

    for (SummaSchemeSymbolName_t* entry = SUMMA_SCHEME_SYMBOL_BUCKETS[index]; entry; entry = entry->next) {
        /* The hash and the length first: both are one comparison, and together
         * they leave the memcmp running only on a genuine candidate. */
        if (entry->hash == hash && entry->length == length && memcmp(entry->value, name, length) == 0) {
            return entry;
        }
    }

    /* One allocation, characters included -- against the two a SummaString
     * costs for its handle and its buffer. */
    SummaSchemeSymbolName_t* const entry = malloc(sizeof(SummaSchemeSymbolName_t) + length + 1);
    assert(entry);
    entry->hash   = hash;
    entry->length = length;
    memcpy(entry->value, name, length + 1);
    entry->next                        = SUMMA_SCHEME_SYMBOL_BUCKETS[index];
    SUMMA_SCHEME_SYMBOL_BUCKETS[index] = entry;
    SUMMA_SCHEME_SYMBOL_COUNT++;

    /* One name per bucket on average. Interning is a read-time cost paid once
     * per distinct name, so the load factor is chosen for lookup rather than
     * for how often the table is rebuilt. */
    if (SUMMA_SCHEME_SYMBOL_COUNT > SUMMA_SCHEME_SYMBOL_BUCKET_COUNT) {
        summa_scheme_symbol_table_grow();
    }
    return entry;
}

#pragma endregion Interned names

SummaSchemeError summa_scheme_procedure_dispatch(SummaSchemeEnvironment env,
                                                 SummaSchemeProcedure   proc,
                                                 const SummaList        args,
                                                 SummaSchemeValue*      out);
SummaSchemeError
summa_scheme_procedure_dispatch_global(SummaSchemeProcedure proc, const SummaList args, SummaSchemeValue* out);
SummaSchemeError
summa_scheme_evaluate_arguments(const SummaSchemeEnvironment env, const SummaList form, SummaList* out);
void summa_scheme_argument_list_free(SummaList args);

/* Frees a list and every value in it. Defined further down, but the reader
 * needs it to unwind a half-built list when a datum inside it fails. */
static void summa_scheme_list_free_deep(SummaList list);

/* What one step of evaluation produced: either the value itself, or an
 * expression the trampoline has to go on and evaluate. The second is what makes
 * tail calls proper -- the step returns rather than recursing, and
 * `summa_scheme_evaluate_inner` loops with the new (environment, expression).
 *
 * Everything in here is owned by whoever holds the step. The trampoline adopts
 * the fields it keeps and calls `summa_scheme_step_release` on the rest.
 *
 * Two of the fields exist only because releasing the frame a tail call leaves
 * can free the expression it is about to evaluate. A procedure body belongs
 * either to a binding -- in which case `body_owner` is the environment holding
 * it, retained -- or to a procedure value the evaluator produced itself, from
 * an operator like `((lambda (x) x) 1)`, in which case `body_operator` is that
 * value, owned outright. `enters_body` says the step moved into a new body and
 * both are the trampoline's now; a step from a special form stays inside the
 * body it was already running and leaves them alone. */
typedef enum {
    SummaSchemeStepValue, /* `*out` holds the answer. */
    SummaSchemeStepTail,  /* Evaluate `expression` in `environment`. */
} SummaSchemeStepKind;

typedef struct {
    SummaSchemeStepKind kind;
    /* Borrowed, and valid only while the body anchors below are held. */
    const SummaSchemeValue* expression;
    SummaSchemeEnvironment  environment;
    bool                    enters_body;
    SummaSchemeEnvironment  body_owner;
    SummaSchemeValue        body_operator;
} SummaSchemeStep;

/* Evaluates form[1..] and either dispatches a builtin or builds the call frame
 * and hands the body's last expression back as a tail step. Every call goes
 * through here. */
static SummaSchemeError summa_scheme_apply(const SummaSchemeEnvironment env,
                                           const SummaSchemeProcedure   proc,
                                           const SummaList              form,
                                           SummaSchemeValue*            out,
                                           SummaSchemeStep*             step);

/* Dispatched before the operands are touched -- a special form decides for
 * itself which of them get evaluated.
 *
 * `step` arrives as a value step and stays one unless the form has a tail
 * position: `if` hands back the taken branch, `let` its last body expression,
 * `and` its last operand, and so on. Handing the expression back rather than
 * evaluating it is the whole of the tail-call optimization -- a form that calls
 * `summa_scheme_evaluate` on a sub-expression in tail position is spending a C
 * frame it does not have to. */
typedef SummaSchemeError (*SummaSchemeSpecialFormFn)(const SummaSchemeEnvironment env,
                                                     const SummaList              form,
                                                     SummaSchemeValue*            out,
                                                     SummaSchemeStep*             step);

static SummaSchemeSpecialFormFn summa_scheme_special_form_lookup(SummaSchemeSymbolName name);

/* Evaluates form[start..] for effect and leaves the last expression to the
 * trampoline. The body rule shared by lambda, begin, let, cond, when and
 * unless, and the reason a procedure body's last expression is a tail
 * position. */
static SummaSchemeError summa_scheme_evaluate_sequence_tail(const SummaSchemeEnvironment env,
                                                            const SummaList              form,
                                                            size_t                       start,
                                                            SummaSchemeValue*            out,
                                                            SummaSchemeStep*             step);

/* The same, run to a value -- for the callers that are not the trampoline. */
static SummaSchemeError summa_scheme_evaluate_sequence(const SummaSchemeEnvironment env,
                                                       const SummaList              form,
                                                       size_t                       start,
                                                       SummaSchemeValue*            out);

#define summa_scheme_step_value() \
    ((SummaSchemeStep){           \
        .kind = SummaSchemeStepValue, .expression = nullptr, .environment = nullptr, .enters_body = false})

/* Both setters acquire, so the step always hands its holder a reference of its
 * own -- including the forms that continue in the environment they were already
 * in, which keeps the trampoline's bookkeeping one shape rather than two. */
static void summa_scheme_step_set_tail(SummaSchemeStep*             step,
                                       const SummaSchemeValue*      expression,
                                       const SummaSchemeEnvironment environment) {
    step->kind        = SummaSchemeStepTail;
    step->expression  = expression;
    step->environment = summa_scheme_environment_acquire(environment);
}

static void summa_scheme_step_release(SummaSchemeStep* step) {
    summa_scheme_environment_release(step->environment);
    summa_scheme_environment_release(step->body_owner);
    summa_scheme_value_free(&step->body_operator);
    *step = summa_scheme_step_value();
}

/* The value R7RS leaves unspecified: `(if #f #f)`, `set!`, `display`. */
#define summa_scheme_unspecified() summa_make_scheme_boolean(false)

typedef enum {
    SummaSchemeReadModeInitial,
    SummaSchemeReadModePlus,
    SummaSchemeReadModeMinus,
    SummaSchemeReadModePeriod,
    SummaSchemeReadModeInteger,
    SummaSchemeReadModeFloating,
    SummaSchemeReadModeSymbol,
    SummaSchemeReadModeString,
} SummaSchemeReadMode;

/* What a single read turned up. A close paren and an exhausted input are both
 * ordinary outcomes inside a list -- one ends it, the other means it was never
 * closed -- and both are errors when the top-level read is what met them, so
 * the decision belongs to the caller rather than to the frame that saw them. */
typedef enum {
    SummaSchemeReadDatumValue, /* `*out` holds the datum. */
    SummaSchemeReadDatumClose, /* A `)`, already consumed. */
    SummaSchemeReadDatumEnd,   /* Input held nothing but atmosphere. */
} SummaSchemeReadDatumKind;

#include <ctype.h>

/* A datum's text ends wherever the next one could begin, so these characters
 * terminate an atom without being part of it. */
static bool summa_scheme_read_is_delimiter(char c) {
    return c == '\0' || isspace(c) || c == '(' || c == ')' || c == '"' || c == ';' || c == '\'';
}

/* Whitespace and `;` line comments carry no datum. Every read skips them
 * first and consumes them afterwards, which is what makes `rest` land on the
 * next datum rather than in the gap before it. Returns the offset of the first
 * character that could start one. */
static size_t summa_scheme_read_skip_atmosphere(const char* input) {
    size_t i = 0;
    for (;;) {
        while (isspace(input[i])) {
            i++;
        }
        if (input[i] != ';') {
            return i;
        }
        /* A line comment runs to the newline, or to the end of input if the
         * last line has none. */
        while (input[i] && input[i] != '\n') {
            i++;
        }
    }
}

SummaSchemeError summa_scheme_read_escape(const char* input, const char** rest, char* out) {
    if (input[0] != '\\') {
        return summa_make_error("summa_scheme_read_escape - input does not start with an escape");
    }

    size_t i     = 1;
    char   value = input[i];
    switch (value) {
    case '\0': {
        return summa_make_error("summa_scheme_read_escape - escape character had no character beyond");
    }
    case 'a': {
        value = '\a';
    } break;
    case 'b': {
        value = '\b';
    } break;
    case 't': {
        value = '\t';
    } break;
    case 'n': {
        value = '\n';
    } break;
    case 'r': {
        value = '\r';
    } break;
    case '"':
    case '\\':
    case '|': {
        /* Stands for itself. */
    } break;
    case 'x': {
        /* `\x<hex>;` -- at least one digit, terminated by a semicolon. */
        unsigned code   = 0;
        size_t   digits = 0;
        while (isxdigit(input[i + 1])) {
            const char digit = input[++i];
            code             = code * 16 + (unsigned)(isdigit(digit) ? digit - '0' : tolower(digit) - 'a' + 10);
            digits++;
            if (code > 0xFF) {
                return summa_make_error("summa_scheme_read_escape - \\x escape is out of byte range");
            }
        }
        if (digits == 0 || input[i + 1] != ';') {
            return summa_make_error("summa_scheme_read_escape - \\x escape wants hex digits then ';'");
        }
        i++; /* The ';' terminator. */
        if (code == 0) {
            /* Tokens are C strings underneath, so an embedded NUL would
             * truncate everything after it. */
            return summa_make_error("summa_scheme_read_escape - \\x escape cannot produce a null character");
        }
        value = (char)code;
    } break;
    default: {
        return summa_make_error("summa_scheme_read_escape - unknown string escape");
    }
    }

    *out = value;
    if (rest) {
        *rest = input + i + 1;
    }
    return summa_success();
}

/* Reads one atom -- a number, symbol, boolean, character or string. `input`
 * must start on the atom's first character, since skipping atmosphere is the
 * caller's job, and `*rest` is left on the first character the atom did not
 * consume. `*rest` is untouched on error. */
static SummaSchemeError summa_scheme_read_atom(const char* input, const char** rest, SummaSchemeValue* out) {
    /* `token` is owned for the whole function, so nothing here returns
     * directly -- a bare `return` leaks it. Every exit sets `err` and jumps to
     * `cleanup`, which frees the token and returns. `err` starts out
     * successful and is only ever assigned on failure, so a success path just
     * fills in `*out` and jumps. */
    SummaSchemeError    err   = summa_success();
    char                c     = '\0';
    size_t              i     = 0;
    SummaSchemeReadMode mode  = SummaSchemeReadModeInitial;
    SummaString         token = summa_string_make_empty();
    for (c = input[0]; c; c = input[++i]) {
        /* Whatever follows `#\` is data, however much it looks like
         * punctuation: `#\(` and `#\;` are characters. */
        const bool after_character_prefix = token->length == 2 && token->value[0] == '#' && token->value[1] == '\\';
        /* Nothing delimits an atom at its first character -- a leading `"`
         * opens a string rather than ending an empty token. */
        if (mode != SummaSchemeReadModeInitial && mode != SummaSchemeReadModeString && !after_character_prefix &&
            summa_scheme_read_is_delimiter(c)) {
            break;
        }

        switch (mode) {
        case SummaSchemeReadModeInitial: {
            switch (c) {
            case '+': {
                mode = SummaSchemeReadModePlus;
            } break;
            case '-': {
                mode = SummaSchemeReadModeMinus;
            } break;
            case '.': {
                mode = SummaSchemeReadModeFloating;
            } break;
            case '"': {
                mode = SummaSchemeReadModeString;
                goto skip_string_push;
            } break;
            default: {
                if (isdigit(c)) {
                    mode = SummaSchemeReadModeInteger;
                } else if (!iscntrl(c)) {
                    mode = SummaSchemeReadModeSymbol;
                } else {
                    err = summa_make_error("summa_scheme_read - SummaSchemeReadModeInitial character not yet handled");
                    goto cleanup;
                }
            }
            }
        } break;
        case SummaSchemeReadModePlus: {
            switch (c) {
            case '.': {
                mode = SummaSchemeReadModeFloating;
            } break;
            default: {
                if (isdigit(c)) {
                    mode = SummaSchemeReadModeInteger;
                    break;
                } else {
                    mode = SummaSchemeReadModeSymbol;
                    break;
                }
            }
            }
        } break;
        case SummaSchemeReadModeMinus: {
            switch (c) {
            case '.': {
                mode = SummaSchemeReadModeFloating;
            } break;
            default: {
                if (isdigit(c)) {
                    mode = SummaSchemeReadModeInteger;
                    break;
                } else {
                    mode = SummaSchemeReadModeSymbol;
                    break;
                }
            }
            }
        } break;
        case SummaSchemeReadModePeriod: {
            switch (c) {
            default: {
                if (isdigit(c)) {
                    mode = SummaSchemeReadModeFloating;
                    break;
                }
                err = summa_make_error("summa_scheme_read - SummaSchemeReadModePeriod character not yet handled");
                goto cleanup;
            }
            }
        } break;
        case SummaSchemeReadModeInteger: {
            switch (c) {
            case '.': {
                mode = SummaSchemeReadModeFloating;
            } break;
            default: {
                if (isdigit(c)) {
                    break;
                }
                err = summa_make_error("summa_scheme_read - SummaSchemeReadModeInteger character not yet handled");
                goto cleanup;
            }
            }
        } break;
        case SummaSchemeReadModeFloating: {
            switch (c) {
            case '.': {
                mode = SummaSchemeReadModeSymbol;
            } break;
            default: {
                if (isdigit(c)) {
                    break;
                }
                err = summa_make_error("summa_scheme_read - SummaSchemeReadModeFloating character not yet handled");
                goto cleanup;
            }
            }
        } break;
        case SummaSchemeReadModeSymbol: {
            // No-op
            break;
        }
        case SummaSchemeReadModeString: {
            switch (c) {
            case '\\': {
                /* The escape is translated, not pushed verbatim, so `\n` in
                 * the source becomes one newline byte in the string. */
                const char* escape_rest = nullptr;
                err                     = summa_scheme_read_escape(input + i, &escape_rest, &c);
                if (err.had) {
                    goto cleanup;
                }
                /* Leave `i` on the escape's last character so the loop's `++i`
                 * steps to whatever follows it. */
                i = (size_t)(escape_rest - input) - 1;
            } break;
            case '"': {
                /* Step past the closing quote so `rest` resumes after it
                 * rather than on it. */
                i++;
                goto token_done;
            } break;
            default:
                break;
            }
        } break;
        default: {
            err = summa_make_error("summa_scheme_read - unhandle read mode");
            goto cleanup;
        }
        }
        if (c) {
            summa_string_push(token, c);
        }
    skip_string_push:
    }
    switch (mode) {
    case SummaSchemeReadModeMinus:
    case SummaSchemeReadModePlus: {
        mode = SummaSchemeReadModeSymbol;
    } break;
    case SummaSchemeReadModeString: {
        err = summa_make_error("summa_scheme_read - string was not closed");
        goto cleanup;
    }
    default:
        break;
    }
token_done:
    // summa_scheme_read_handle_token_finish:
    switch (mode) {
    case SummaSchemeReadModeInteger: {
        int64_t value = atoll(token->value);
        *out          = summa_make_scheme_integer(value);
        goto cleanup;
    }
    case SummaSchemeReadModeFloating: {
        if (token->length == 1) {
            err = summa_make_error("summa_scheme_read - floating point number cannot be a sole decimal");
            goto cleanup;
        }
        double value = atof(token->value);
        *out         = summa_make_scheme_floating(value);
        goto cleanup;
    }
    case SummaSchemeReadModeString: {
        *out = summa_make_scheme_string(token->value);
        goto cleanup;
    }
    case SummaSchemeReadModeSymbol: {
        if (token->value[0] == '#') { // Special object parsing
            if (token->length == 2) { // Boolean parsing
                if (strncmp(token->value, SUMMA_SCHEME_TRUE, 2) == 0) {
                    *out = summa_make_scheme_boolean(true);
                    goto cleanup;
                } else if (strncmp(token->value, SUMMA_SCHEME_FALSE, 2) == 0) {
                    *out = summa_make_scheme_boolean(false);
                    goto cleanup;
                }
            } else if (token->value[1] == '\\') { // Character parsing
                if (token->length == 3) {         // Normal character
                    *out = summa_make_scheme_character(token->value[2]);
                    goto cleanup;
                } else if (strcmp(token->value, "#\\space") == 0) {
                    *out = summa_make_scheme_character(' ');
                    goto cleanup;
                } else if (strcmp(token->value, "#\\newline") == 0) {
                    *out = summa_make_scheme_character('\n');
                    goto cleanup;
                } else if (strcmp(token->value, "#\\tab") == 0) {
                    *out = summa_make_scheme_character('\t');
                    goto cleanup;
                } else {
                    err = summa_make_error("summa_scheme_read - unknown character name");
                    goto cleanup;
                }
            } else {
                err = summa_make_error("summa_scheme_read - unknown # object");
                goto cleanup;
            }
        }
        *out = summa_make_scheme_symbol(token->value);
        goto cleanup;
    }
    default: {
        err = summa_make_error("summa_scheme_read - UNREACHABLE");
        goto cleanup;
    }
    }

cleanup:
    if (!err.had && rest) {
        *rest = input + i;
    }
    summa_string_free(token);
    return err;
}

/* Reads datums up to the closing paren, which is the shared body of a list and
 * a vector. `input` starts just past the opening bracket. */
static SummaSchemeError summa_scheme_read_sequence(const char* input, const char** rest, SummaList* out, size_t depth);

/* Reads one datum. `kind` reports what was found, since a `)` and an exhausted
 * input are both ordinary outcomes one frame down and errors at the top. */
static SummaSchemeError summa_scheme_read_datum(
    const char* input, const char** rest, SummaSchemeValue* out, SummaSchemeReadDatumKind* kind, size_t depth) {
    if (depth > SUMMA_SCHEME_READ_MAX_DEPTH) {
        return summa_make_error("summa_scheme_read - nesting is too deep");
    }

    const char* cursor = input + summa_scheme_read_skip_atmosphere(input);

    if (!*cursor) {
        *kind = SummaSchemeReadDatumEnd;
        if (rest) {
            *rest = cursor;
        }
        return summa_success();
    }
    if (*cursor == ')') {
        *kind = SummaSchemeReadDatumClose;
        if (rest) {
            *rest = cursor + 1;
        }
        return summa_success();
    }

    *kind = SummaSchemeReadDatumValue;
    switch (*cursor) {
    case '(': {
        SummaList        items = nullptr;
        SummaSchemeError err   = summa_scheme_read_sequence(cursor + 1, &cursor, &items, depth + 1);
        if (err.had) {
            return err;
        }
        *out = summa_make_scheme_list(items);
    } break;
    case '\'': {
        /* `'x` expands to `(quote x)` here, so the shorthand never reaches the
         * evaluator as a form of its own. */
        SummaSchemeValue         quoted      = {};
        SummaSchemeReadDatumKind quoted_kind = SummaSchemeReadDatumValue;
        SummaSchemeError         err = summa_scheme_read_datum(cursor + 1, &cursor, &quoted, &quoted_kind, depth + 1);
        if (err.had) {
            return err;
        }
        if (quoted_kind != SummaSchemeReadDatumValue) {
            return summa_make_error("summa_scheme_read - quote has nothing to quote");
        }
        SummaList form = summa_list_make_empty();
        summa_list_push(form, &summa_make_scheme_symbol("quote"));
        summa_list_push(form, &quoted);
        *out = summa_make_scheme_list(form);
    } break;
    default: {
        /* `#` starts a vector only when a bracket follows; otherwise it is the
         * first character of `#t`, `#f` or `#\c`. */
        if (cursor[0] == '#' && cursor[1] == '(') {
            SummaList        items = nullptr;
            SummaSchemeError err   = summa_scheme_read_sequence(cursor + 2, &cursor, &items, depth + 1);
            if (err.had) {
                return err;
            }
            *out = summa_make_scheme_vector(items);
            break;
        }
        SummaSchemeError err = summa_scheme_read_atom(cursor, &cursor, out);
        if (err.had) {
            return err;
        }
    } break;
    }

    if (rest) {
        *rest = cursor + summa_scheme_read_skip_atmosphere(cursor);
    }
    return summa_success();
}

static SummaSchemeError
summa_scheme_read_sequence(const char* input, const char** rest, SummaList* out, size_t depth) {
    /* `items` is owned until it is handed over, so the error paths unwind it
     * along with every datum already pushed into it. */
    SummaSchemeError err    = summa_success();
    SummaList        items  = summa_list_make_empty();
    const char*      cursor = input;
    for (;;) {
        SummaSchemeValue         item = {};
        SummaSchemeReadDatumKind kind = SummaSchemeReadDatumValue;

        err = summa_scheme_read_datum(cursor, &cursor, &item, &kind, depth);
        if (err.had) {
            goto cleanup;
        }
        if (kind == SummaSchemeReadDatumClose) {
            break;
        }
        if (kind == SummaSchemeReadDatumEnd) {
            err = summa_make_error("summa_scheme_read - input ended inside a list");
            goto cleanup;
        }
        summa_list_push(items, &item);
    }

    *out  = items;
    items = nullptr; /* Handed to the caller; no longer ours to free. */
    if (rest) {
        *rest = cursor;
    }
cleanup:
    summa_scheme_list_free_deep(items);
    return err;
}

SummaSchemeError summa_scheme_read([[maybe_unused]] const SummaSchemeEnvironment env,
                                   const char*                                   input,
                                   const char**                                  rest,
                                   SummaSchemeValue*                             out) {
    const char*              cursor = nullptr;
    SummaSchemeValue         value  = {};
    SummaSchemeReadDatumKind kind   = SummaSchemeReadDatumValue;

    const SummaSchemeError err = summa_scheme_read_datum(input, &cursor, &value, &kind, 0);
    if (err.had) {
        return err;
    }
    if (kind == SummaSchemeReadDatumEnd) {
        return summa_make_error("summa_scheme_read - input held no datum");
    }
    if (kind == SummaSchemeReadDatumClose) {
        return summa_make_error("summa_scheme_read - unbalanced close paren");
    }

    /* Nothing else in the grammar can start with `)`, so one sitting where the
     * next datum would go is unbalanced no matter what preceded it. Caught
     * here rather than left for the next call, so `(1))` fails as a whole. */
    if (*cursor == ')') {
        summa_scheme_value_free(&value);
        return summa_make_error("summa_scheme_read - unbalanced close paren");
    }

    *out = value;
    if (rest) {
        *rest = cursor;
    }
    return summa_success();
}

bool summa_scheme_truthy(const SummaSchemeValue* value) {
    return !(value->type == SummaSchemeBooleanType && !value->value.boolean.value);
}

/* Renders a value short enough to sit inside an error message. Atoms print
 * themselves; the containers name their type rather than dumping their
 * contents, since the operator is what the reader needs to see. */
static void summa_scheme_describe(const SummaSchemeValue* value, char* buffer, size_t size) {
    switch (value->type) {
    case SummaSchemeBooleanType: {
        snprintf(buffer, size, "%s", value->value.boolean.value ? SUMMA_SCHEME_TRUE : SUMMA_SCHEME_FALSE);
    } break;
    case SummaSchemeCharacterType: {
        snprintf(buffer, size, "#\\%c", value->value.character.value);
    } break;
    case SummaSchemeFloatingType: {
        snprintf(buffer, size, "%f", value->value.floating.value);
    } break;
    case SummaSchemeIntegerType: {
        snprintf(buffer, size, "%" PRId64, value->value.integer.value);
    } break;
    case SummaSchemeListType: {
        snprintf(buffer, size, "a list");
    } break;
    case SummaSchemeProcedureType: {
        snprintf(buffer, size, "#<procedure %s>", value->value.procedure.name->value);
    } break;
    case SummaSchemeStringType: {
        snprintf(buffer, size, "\"%s\"", value->value.string.value->value);
    } break;
    case SummaSchemeSymbolType: {
        snprintf(buffer, size, "%s", value->value.symbol.value->value);
    } break;
    case SummaSchemeVectorType: {
        snprintf(buffer, size, "a vector");
    } break;
    }
}

#define SUMMA_SCHEME_DESCRIPTION_LENGTH 128

static SummaSchemeError summa_scheme_wrong_type_to_apply(const SummaSchemeValue* operator_value) {
    char described[SUMMA_SCHEME_DESCRIPTION_LENGTH];
    summa_scheme_describe(operator_value, described, sizeof(described));
    snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "Wrong type to apply: %s", described);
    return summa_make_error(ERROR_MESSAGE);
}

static size_t SUMMA_SCHEME_DEPTH = 0;

static SummaSchemeError
summa_scheme_evaluate_inner(const SummaSchemeEnvironment env, const SummaSchemeValue in, SummaSchemeValue* out);

/* Every *non-tail* recursion re-enters here, so the depth guard is written
 * once. A tail call never gets this far -- it loops inside
 * `summa_scheme_evaluate_inner` and costs no depth. */
SummaSchemeError
summa_scheme_evaluate(const SummaSchemeEnvironment env, const SummaSchemeValue in, SummaSchemeValue* out) {
    if (!out) {
        return summa_make_error("summa_scheme_evaluate - Out file was null");
    }
    /* Seeded before anything can fail, so every path out has written *out and
     * no error path has to remember to. */
    *out = summa_scheme_unspecified();

    if (SUMMA_SCHEME_DEPTH >= SUMMA_SCHEME_MAX_DEPTH) {
        return summa_make_error(
            "summa_scheme_evaluate - recursion limit exceeded (non-tail recursion needs a C frame per level)");
    }
    SUMMA_SCHEME_DEPTH++;
    const SummaSchemeError err = summa_scheme_evaluate_inner(env, in, out);
    SUMMA_SCHEME_DEPTH--;
    return err;
}

/* One step: either the answer, or the expression the trampoline goes round
 * again with. Nothing here recurses in tail position. */
static SummaSchemeError summa_scheme_evaluate_step(const SummaSchemeEnvironment env,
                                                   const SummaSchemeValue*      in,
                                                   SummaSchemeValue*            out,
                                                   SummaSchemeStep*             step) {
    switch (in->type) {
    case SummaSchemeBooleanType: {
        *out = summa_make_scheme_boolean(in->value.boolean.value);
    } break;
    case SummaSchemeCharacterType: {
        *out = summa_make_scheme_character(in->value.character.value);
    } break;
    case SummaSchemeFloatingType: {
        *out = summa_make_scheme_floating(in->value.floating.value);
    } break;
    case SummaSchemeIntegerType: {
        *out = summa_make_scheme_integer(in->value.integer.value);
    } break;
    case SummaSchemeListType: {
        /* A list in evaluated position is a combination, never data. `(1 2 3)`
         * is an error, and the way to get the list itself is to quote it. */
        const SummaList form = in->value.list.value;
        if (!form || form->length == 0) {
            return summa_make_error("Illegal empty combination: ()");
        }

        if (form->value[0].type == SummaSchemeSymbolType) {
            /* Ahead of the binding lookup, deliberately. `if` routed through
             * the application path would evaluate both branches and turn every
             * recursive base case into a loop. */
            const SummaSchemeSpecialFormFn special =
                summa_scheme_special_form_lookup(form->value[0].value.symbol.value);
            if (special) {
                return special(env, form, out, step);
            }

            /* Resolved through the binding rather than by evaluating the
             * symbol, which would deep-copy the procedure on every call. */
            SummaSchemeBinding     head;
            SummaSchemeEnvironment owner = nullptr;
            const SummaSchemeError lookup =
                summa_scheme_environment_get_owner(env, form->value[0].value.symbol.value, &head, &owner);
            if (lookup.had) {
                return lookup;
            }
            if (head.value.type != SummaSchemeProcedureType) {
                return summa_scheme_wrong_type_to_apply(&head.value);
            }

            /* Retained across the call, because the call may be a tail call and
             * the trampoline is about to release the frame this binding lives
             * in -- which would free the very body it is about to run. A
             * refcount bump, not a copy of the body. */
            const SummaSchemeEnvironment holder = summa_scheme_environment_acquire(owner);
            const SummaSchemeError       err    = summa_scheme_apply(env, head.value.value.procedure, form, out, step);
            if (!err.had && step->kind == SummaSchemeStepTail) {
                step->enters_body = true;
                step->body_owner  = holder;
            } else {
                summa_scheme_environment_release(holder);
            }
            return err;
        }

        /* Any other operator is an expression: `((lambda (x) x) 1)`, and also
         * `(1 2 3)`, where the 1 evaluates to itself and then fails to apply.
         * The operator is not a tail position, so this one recurses. */
        SummaSchemeValue operator_value;
        SummaSchemeError err = summa_scheme_evaluate(env, form->value[0], &operator_value);
        if (err.had) {
            return err;
        }
        if (operator_value.type != SummaSchemeProcedureType) {
            err = summa_scheme_wrong_type_to_apply(&operator_value);
            summa_scheme_value_free(&operator_value);
            return err;
        }
        err = summa_scheme_apply(env, operator_value.value.procedure, form, out, step);
        if (!err.had && step->kind == SummaSchemeStepTail) {
            /* No binding owns this one, so the value itself is what holds the
             * body up. Moved into the step rather than freed. */
            step->enters_body   = true;
            step->body_operator = operator_value;
        } else {
            summa_scheme_value_free(&operator_value);
        }
        return err;
    } break;
    case SummaSchemeProcedureType: {
        return summa_scheme_value_copy(out, in);
    } break;
    case SummaSchemeStringType: {
        *out = summa_make_scheme_string(in->value.string.value->value);
    } break;
    case SummaSchemeSymbolType: {
        /* A symbol in evaluated position is a variable reference, so an unbound
         * one is an error rather than a value. Quoting is what yields the
         * symbol itself. */
        SummaSchemeBinding     binding;
        const SummaSchemeError err = summa_scheme_environment_get(env, in->value.symbol, &binding);
        if (err.had) {
            return err;
        }
        /* A copy the caller owns; the binding's value stays the environment's. */
        return summa_scheme_value_copy(out, &binding.value);
    } break;
    case SummaSchemeVectorType: {
        /* Self-evaluating, unlike a list. */
        return summa_scheme_value_copy(out, in);
    } break;
    default: {
        return summa_make_error("summa_scheme_evaluate - Invalid in type");
    }
    }

    return summa_success();
}

/* The trampoline. A step that lands in tail position hands back
 * (environment, expression) instead of recursing, and this loops with them --
 * so a tail call costs one iteration here rather than a C frame, and a tail
 * loop runs forever without growing the stack.
 *
 * Three references are held across an iteration, and the order they are swapped
 * in is the whole correctness argument:
 *
 * - `environment` is where the next expression is evaluated. Every step hands
 *   back a reference of its own, so the frame being left is released *after*
 *   the one replacing it is in hand.
 * - `body_owner` and `body_value` keep the expression itself alive. It is
 *   borrowed -- a pointer into some procedure's body -- and the frame this loop
 *   is releasing may be that body's only owner. See `SummaSchemeStep`.
 *
 * Nothing accumulates: one reference in, one out, per iteration. A million-step
 * tail loop leaves the live environment count exactly where it found it. */
static SummaSchemeError
summa_scheme_evaluate_inner(const SummaSchemeEnvironment env, const SummaSchemeValue in, SummaSchemeValue* out) {
    const SummaSchemeValue* expression  = &in;
    SummaSchemeEnvironment  environment = summa_scheme_environment_acquire(env);
    SummaSchemeEnvironment  body_owner  = nullptr;
    SummaSchemeValue        body_value  = {};
    SummaSchemeError        err         = summa_success();

    for (;;) {
        SummaSchemeStep step = summa_scheme_step_value();

        err = summa_scheme_evaluate_step(environment, expression, out, &step);
        if (err.had || step.kind == SummaSchemeStepValue) {
            summa_scheme_step_release(&step);
            break;
        }

        if (step.enters_body) {
            /* The step already acquired what replaces these, so releasing them
             * here cannot pull the new body out from under itself. */
            summa_scheme_environment_release(body_owner);
            summa_scheme_value_free(&body_value);
            body_owner = step.body_owner;
            body_value = step.body_operator;
        }
        summa_scheme_environment_release(environment);
        environment = step.environment;
        expression  = step.expression;
    }

    summa_scheme_environment_release(environment);
    summa_scheme_environment_release(body_owner);
    summa_scheme_value_free(&body_value);
    return err;
}

static SummaSchemeError summa_scheme_print_styled(const SummaSchemeValue value, FILE* out, bool readable);

SummaSchemeError summa_scheme_print(const SummaSchemeValue value, FILE* out) {
    return summa_scheme_print_styled(value, out, true);
}

SummaSchemeError summa_scheme_display(const SummaSchemeValue value, FILE* out) {
    return summa_scheme_print_styled(value, out, false);
}

static SummaSchemeError summa_scheme_print_styled(const SummaSchemeValue value, FILE* out, bool readable) {
    if (!out) {
        return summa_make_error("summa_scheme_print - Out file was null");
    }

    switch (value.type) {
    case SummaSchemeBooleanType: {
        SummaSchemeBoolean val = value.value.boolean;
        if (val.value) {
            fprintf(out, SUMMA_SCHEME_TRUE);
        } else {
            fprintf(out, SUMMA_SCHEME_FALSE);
        }
    } break;
    case SummaSchemeCharacterType: {
        const SummaSchemeCharacter val = value.value.character;
        if (!readable) {
            /* `display` puts the character itself in the stream -- writing a
             * newline means a line break, not the six letters of its name. */
            fprintf(out, "%c", val.value);
            break;
        }
        /* `write` has to read back, and the three the reader spells by name
         * are exactly the three whose glyph would not survive the round trip.
         * R7RS names more of them -- `#\null`, `#\return`, `#\alarm`,
         * `#\delete` -- but summa_scheme_read_atom accepts only these, so
         * emitting the others would produce source it could not read. */
        const char* name = nullptr;
        switch (val.value) {
        case ' ': {
            name = "space";
        } break;
        case '\n': {
            name = "newline";
        } break;
        case '\t': {
            name = "tab";
        } break;
        default:
            break;
        }
        if (name) {
            fprintf(out, "#\\%s", name);
        } else {
            fprintf(out, "#\\%c", val.value);
        }
    } break;
    case SummaSchemeFloatingType: {
        SummaSchemeFloating val = value.value.floating;
        fprintf(out, "%f", val.value);
    } break;
    case SummaSchemeIntegerType: {
        SummaSchemeInteger val = value.value.integer;
        fprintf(out, "%" PRId64, val.value);
    } break;
    case SummaSchemeListType: {
        SummaSchemeList val = value.value.list;
        fprintf(out, "(");
        for (size_t i = 0; i < val.value->length; i++) {
            if (i != 0) {
                fprintf(out, " ");
            }
            SummaSchemeValue next_value = val.value->value[i];
            summa_scheme_print_styled(next_value, out, readable);
        }
        fprintf(out, ")");
    } break;
    case SummaSchemeProcedureType: {
        fprintf(out, "#<procedure %s (", value.value.procedure.name->value);
        SummaSchemeSymbolList bindings = value.value.procedure.bindings;
        for (size_t i = 0; i < bindings->length; i++) {
            SummaSchemeSymbol binding = bindings->value[i];
            if (i != 0) {
                fprintf(out, " %s", binding.value->value);
            } else {
                fprintf(out, "%s", binding.value->value);
            }
        }
        fprintf(out, ")>");
    } break;
    case SummaSchemeStringType: {
        SummaSchemeString val = value.value.string;
        SummaString       str = val.value;
        fprintf(out, readable ? "\"%s\"" : "%s", str->value);
    } break;
    case SummaSchemeSymbolType: {
        fprintf(out, "%s", value.value.symbol.value->value);
    } break;
    case SummaSchemeVectorType: {
        SummaSchemeVector val = value.value.vector;
        fprintf(out, "#(");
        for (size_t i = 0; i < val.value->length; i++) {
            if (i != 0) {
                fprintf(out, " ");
            }
            SummaSchemeValue next_value = val.value->value[i];
            summa_scheme_print_styled(next_value, out, readable);
        }
        fprintf(out, ")");
    } break;
    default: {
        return summa_make_error("summa_scheme_print - Invalid value type");
    }
    }

    return summa_success();
}

/* summa_list_copy moves elements as raw bytes, leaving the copy sharing every
 * handle inside them. These walk instead.
 *
 * The length is known before the first element is copied, so the destination is
 * one exact allocation. Built from `make_empty` it was five reallocs and 320
 * bytes of slack for a 128-element list -- the same over-allocation
 * `summa_scheme_environment_make_with_capacity` stopped paying for a frame, on
 * the one remaining copy whose cost is O(the list). */
static SummaList summa_scheme_list_copy_deep(const SummaList src) {
    if (!src) {
        return nullptr;
    }
    SummaList dest = summa_list_make_with_capacity(src->length);
    for (size_t i = 0; i < src->length; i++) {
        SummaSchemeValue element;
        summa_scheme_value_copy(&element, &src->value[i]);
        summa_list_push(dest, &element);
    }
    return dest;
}

/* Shallow, unlike its list counterpart, and that is what interning bought: a
 * parameter name is a pointer into the intern table, so copying a procedure's
 * parameter list copies pointers rather than allocating a string per name. */
static SummaSchemeSymbolList summa_scheme_symbol_list_copy(const SummaSchemeSymbolList src) {
    if (!src) {
        return nullptr;
    }
    SummaSchemeSymbolList dest = summa_symbol_list_make_empty();
    for (size_t i = 0; i < src->length; i++) {
        summa_symbol_list_push(dest, &src->value[i]);
    }
    return dest;
}

static void summa_scheme_list_free_deep(SummaList list) {
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->length; i++) {
        summa_scheme_value_free(&list->value[i]);
    }
    summa_list_free(list);
}

SummaSchemeError summa_scheme_value_copy(SummaSchemeValue* dest, const SummaSchemeValue* src) {
    SummaSchemeValueType type = src->type;
    dest->type                = type;
    switch (type) {
    case SummaSchemeBooleanType: {
        dest->value = src->value;
    } break;
    case SummaSchemeCharacterType: {
        dest->value = src->value;
    } break;
    case SummaSchemeFloatingType: {
        dest->value = src->value;
    } break;
    case SummaSchemeIntegerType: {
        dest->value = src->value;
    } break;
    case SummaSchemeListType: {
        dest->value.list.value = summa_scheme_list_copy_deep(src->value.list.value);
    } break;
    case SummaSchemeProcedureType: {
        /* dest's handles are whatever the caller happened to have there, so
         * each one is built fresh before being copied into -- the same shape
         * the list and vector branches use. A procedure may legitimately carry
         * no bindings or no body, and a null handle stays null. */
        dest->value.procedure.name     = src->value.procedure.name; /* Interned; borrowed. */
        dest->value.procedure.bindings = nullptr;
        dest->value.procedure.body     = nullptr;
        /* Retained: the copy is another thing pointing at that environment, and
         * the count is what keeps the environment there for it. Released again
         * by summa_scheme_value_free -- the two are the counted edge set, and
         * summa_scheme_environment_for_each_edge has to enumerate exactly it. */
        dest->value.procedure.closure = summa_scheme_environment_acquire(src->value.procedure.closure);
        if (src->value.procedure.bindings) {
            dest->value.procedure.bindings = summa_scheme_symbol_list_copy(src->value.procedure.bindings);
        }
        if (src->value.procedure.body) {
            dest->value.procedure.body = summa_scheme_list_copy_deep(src->value.procedure.body);
        }
    } break;
    case SummaSchemeStringType: {
        dest->value.string.value = summa_string_make(src->value.string.value->value);
    } break;
    case SummaSchemeSymbolType: {
        /* Interned, so the copy is the pointer. Nothing to allocate here and
         * nothing for summa_scheme_value_free to give back. */
        dest->value.symbol.value = src->value.symbol.value;
    } break;
    case SummaSchemeVectorType: {
        dest->value.vector.value = summa_scheme_list_copy_deep(src->value.vector.value);
    } break;
    default: {
        return summa_make_error("summa_scheme_value_copy - Invalid scheme type provided");
    } break;
    }

    return summa_success();
}

/* Releases a handle and nulls it, so a double free through the same value is a
 * no-op rather than a crash. */
#define SUMMA_SCHEME_FREE_HANDLE(handle, free_fn) \
    do {                                          \
        if (handle) {                             \
            free_fn(handle);                      \
            (handle) = nullptr;                   \
        }                                         \
    } while (0)

void summa_scheme_value_free(SummaSchemeValue* value) {
    if (!value) {
        return;
    }
    switch (value->type) {
    case SummaSchemeBooleanType:
    case SummaSchemeCharacterType:
    case SummaSchemeFloatingType:
    case SummaSchemeIntegerType: {
        /* Stored inline in the union; nothing was allocated. */
    } break;
    case SummaSchemeSymbolType: {
        /* The name is the intern table's, for the life of the process. A value
         * naming it borrowed it, so there is nothing to give back -- the
         * mirror of summa_scheme_value_copy's symbol case. */
    } break;
    case SummaSchemeListType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.list.value, summa_scheme_list_free_deep);
    } break;
    case SummaSchemeProcedureType: {
        /* A procedure can carry no bindings or no body, so each handle is
         * checked before release. `closure` is one of them: the procedure held
         * a reference from the moment it was made or copied, and this gives it
         * back. An environment tearing itself down has already nulled the slot
         * through summa_scheme_environment_for_each_edge, so that path releases
         * it once rather than twice.
         *
         * `name` is interned and so is every entry of `bindings`, which is why
         * only the list itself is freed and the name is not touched at all. */
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.closure, summa_scheme_environment_release);
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.bindings, summa_symbol_list_free);
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.body, summa_scheme_list_free_deep);
    } break;
    case SummaSchemeStringType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.string.value, summa_string_free);
    } break;
    case SummaSchemeVectorType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.vector.value, summa_scheme_list_free_deep);
    } break;
    }
}

bool summa_scheme_value_equals(const SummaSchemeValue* left, const SummaSchemeValue* right) {
    if (left == right) {
        return true;
    }
    if (left == nullptr || right == nullptr) {
        return false;
    }
    if (left->type != right->type) {
        return false;
    }
    const SummaSchemeValueType type = left->type;
    switch (type) {
    case SummaSchemeBooleanType: {
        return left->value.boolean.value == right->value.boolean.value;
    }
    case SummaSchemeCharacterType: {
        return left->value.character.value == right->value.character.value;
    }
    case SummaSchemeFloatingType: {
        return left->value.floating.value == right->value.floating.value;
    }
    case SummaSchemeIntegerType: {
        return left->value.integer.value == right->value.integer.value;
    }
    case SummaSchemeListType: {
        SummaList left_list  = left->value.list.value;
        SummaList right_list = right->value.list.value;
        if (left_list->length != right_list->length) {
            return false;
        }
        for (size_t i = 0; i < left_list->length; i++) {
            if (!summa_scheme_value_equals(left_list->value + i, right_list->value + i)) {
                return false;
            }
        }
        return true;
    }
    case SummaSchemeProcedureType: {
        return left->value.procedure.name == right->value.procedure.name;
    }
    case SummaSchemeStringType: {
        /* Strings are values rather than names, so this one stays a compare of
         * contents: `(string-ref s 0)` builds them at runtime and two equal
         * strings are two allocations. */
        return summa_string_cmp(left->value.string.value, right->value.string.value) == 0;
    }
    case SummaSchemeSymbolType: {
        /* Interning is what makes this a pointer compare. Two symbols are the
         * same symbol exactly when they name the same record. */
        return left->value.symbol.value == right->value.symbol.value;
    }
    case SummaSchemeVectorType: {
        SummaList leftVector  = left->value.vector.value;
        SummaList rightVector = right->value.vector.value;
        if (leftVector->length != rightVector->length) {
            return false;
        }
        for (size_t i = 0; i < leftVector->length; i++) {
            if (!summa_scheme_value_equals(leftVector->value + i, rightVector->value + i)) {
                return false;
            }
        }
        return true;
    }
    default: {
        return false;
    }
    }
}

#pragma region Environment lifetime

SUMMA_ARRAY_GENERATE_TYPE(SummaSchemeEnvironmentList, environment_list, SummaSchemeEnvironment)

/* ── The counted edge set ──────────────────────────────────────────────────
 *
 * An environment's outgoing references to other environments are its `parent`
 * and the `closure` of every procedure its bindings can reach -- including
 * procedures nested inside a bound list or vector, and inside another
 * procedure's body, because summa_scheme_value_copy acquires those too.
 *
 * Exactly one function walks that set, and both sides of the design call it.
 * Teardown releases what it enumerates; the collector subtracts, marks and
 * breaks what it enumerates. That is deliberate. Trial deletion's one failure
 * mode is edge-set drift -- a reference the destructor releases but the
 * collector does not walk (or the reverse) makes the collector's arithmetic
 * wrong, and wrong arithmetic frees live data whose corruption then surfaces
 * somewhere else entirely. Two traversals that must agree will eventually stop
 * agreeing; one traversal cannot disagree with itself.
 *
 * The visitor receives the *slot*, not the value, so a visitor that releases
 * can null the slot in the same step. That is what keeps teardown from
 * double-releasing: once a closure slot is null, the ordinary
 * summa_scheme_value_free that follows has nothing left to give back.
 *
 * The invariant, stated once: a reference this enumerates is a reference
 * summa_scheme_value_copy acquired and summa_scheme_value_free would release.
 * Adding a value type that can carry an environment means adding it here in the
 * same commit. */
typedef void (*SummaSchemeEnvironmentEdgeFn)(SummaSchemeEnvironment* slot, void* context);

static void
summa_scheme_list_for_each_environment_edge(SummaList list, SummaSchemeEnvironmentEdgeFn visit, void* context);

static void summa_scheme_value_for_each_environment_edge(SummaSchemeValue*            value,
                                                         SummaSchemeEnvironmentEdgeFn visit,
                                                         void*                        context) {
    switch (value->type) {
    case SummaSchemeProcedureType: {
        visit(&value->value.procedure.closure, context);
        /* The body is deep-copied like any other list, so a procedure value
         * sitting in it carries a counted closure of its own. */
        summa_scheme_list_for_each_environment_edge(value->value.procedure.body, visit, context);
    } break;
    case SummaSchemeListType: {
        summa_scheme_list_for_each_environment_edge(value->value.list.value, visit, context);
    } break;
    case SummaSchemeVectorType: {
        summa_scheme_list_for_each_environment_edge(value->value.vector.value, visit, context);
    } break;
    case SummaSchemeBooleanType:
    case SummaSchemeCharacterType:
    case SummaSchemeFloatingType:
    case SummaSchemeIntegerType:
    case SummaSchemeStringType:
    case SummaSchemeSymbolType: {
        /* Nothing that can point at an environment. A symbol names an interned
         * record, which is not counted and not owned, so it is not an edge. */
    } break;
    }
}

static void
summa_scheme_list_for_each_environment_edge(SummaList list, SummaSchemeEnvironmentEdgeFn visit, void* context) {
    for (size_t i = 0; list && i < list->length; i++) {
        summa_scheme_value_for_each_environment_edge(&list->value[i], visit, context);
    }
}

static void summa_scheme_environment_for_each_edge(const SummaSchemeEnvironment env,
                                                   SummaSchemeEnvironmentEdgeFn visit,
                                                   void*                        context) {
    visit(&env->parent, context);
    for (size_t i = 0; env->bindings && i < env->bindings->length; i++) {
        summa_scheme_value_for_each_environment_edge(&env->bindings->value[i].value, visit, context);
    }
}

/* ── The registry ──────────────────────────────────────────────────────────
 *
 * Every live environment, in one intrusive doubly linked list. Trial deletion
 * has no roots to start from, so it starts from all of them; the list is what
 * "all of them" means, and its length is what live_count reports. Intrusive so
 * that linking and unlinking are O(1) on a path -- environment_make and the
 * destructor -- that runs once per call frame.
 *
 * Single-threaded, like SUMMA_SCHEME_DEPTH. */
static SummaSchemeEnvironment SUMMA_SCHEME_ENVIRONMENT_REGISTRY = nullptr;
static size_t                 SUMMA_SCHEME_LIVE_ENVIRONMENTS    = 0;

/* ── When collection runs ──────────────────────────────────────────────────
 *
 * A collector nobody triggers is not a collector, so there are three ways in
 * and the automatic one carries the load:
 *
 *  - summa_scheme_environment_make runs a pass once the registry reaches
 *    SUMMA_SCHEME_GC_THRESHOLD entries. Allocation is the only moment the
 *    garbage can grow, so it is the only moment worth checking, and doing it
 *    before the new environment exists keeps a half-built one out of the pass.
 *  - summa_scheme_environment_free runs one unconditionally. A program that
 *    never made 64 environments would otherwise end with its cycles still
 *    counted, and live_count would read as a leak for the rest of the process.
 *  - summa_scheme_collect_cycles is public for a host that wants a pass at a
 *    moment of its own choosing.
 *
 * Every pass retunes the threshold to twice what survived it, never below the
 * floor. A pass costs time proportional to the registry, so scaling the gap to
 * the live set keeps the amortized cost per environment constant and bounds the
 * garbage in flight to a constant factor of what is genuinely alive. Retuning
 * on every pass rather than only on the automatic ones is what lets the
 * threshold come back *down*: a program that briefly held thousands of frames
 * must not leave the next one collecting only once in thousands. The floor
 * stops a program with a handful of environments from collecting on every
 * frame. */
#define SUMMA_SCHEME_GC_MIN_THRESHOLD 64
static size_t SUMMA_SCHEME_GC_THRESHOLD = SUMMA_SCHEME_GC_MIN_THRESHOLD;
static bool   SUMMA_SCHEME_GC_RUNNING   = false;

size_t summa_scheme_environment_live_count(void) {
    return SUMMA_SCHEME_LIVE_ENVIRONMENTS;
}

/* The counter sits one header ahead of the handle: ref_count carves the payload
 * out of the same allocation, so `ref_count_as(rc, SummaSchemeEnvironment)` is
 * `rc + 1` and this is the way back. */
static RefCount summa_scheme_environment_ref(const SummaSchemeEnvironment env) {
    return (RefCount)(void*)env - 1;
}

SummaSchemeEnvironment summa_scheme_environment_acquire(const SummaSchemeEnvironment env) {
    if (env) {
        ref_count_acquire(summa_scheme_environment_ref(env));
    }
    return env;
}

void summa_scheme_environment_release(const SummaSchemeEnvironment env) {
    if (env) {
        ref_count_release(summa_scheme_environment_ref(env));
    }
}

/* Give back one edge and null the slot, so whatever frees the value afterwards
 * sees nothing left to release. Used by teardown and by the collector's
 * breaking step -- the same operation in both, which is the point. */
static void summa_scheme_environment_edge_release(SummaSchemeEnvironment* slot, [[maybe_unused]] void* context) {
    if (*slot) {
        const SummaSchemeEnvironment target = *slot;
        *slot                               = nullptr;
        summa_scheme_environment_release(target);
    }
}

/* ref_count runs this on the release that reaches zero, on the payload rather
 * than the header. Reentrant by design: releasing the parent, or a binding
 * holding the last reference to some other environment, lands back here for
 * that one. The recursion is the program's own environment chain, which the
 * evaluator's depth guard already bounds. */
static void summa_scheme_environment_destroy(void* payload) {
    const SummaSchemeEnvironment env = payload;

    if (env->registry_previous) {
        env->registry_previous->registry_next = env->registry_next;
    } else {
        SUMMA_SCHEME_ENVIRONMENT_REGISTRY = env->registry_next;
    }
    if (env->registry_next) {
        env->registry_next->registry_previous = env->registry_previous;
    }
    env->registry_previous = nullptr;
    env->registry_next     = nullptr;
    SUMMA_SCHEME_LIVE_ENVIRONMENTS--;

    /* Every outgoing reference, through the one enumeration the collector also
     * uses -- released and nulled before anything below can free the value it
     * was reached through. */
    summa_scheme_environment_for_each_edge(env, summa_scheme_environment_edge_release, nullptr);

    /* The value only: a binding's name is interned, so the environment never
     * owned it. */
    for (size_t i = 0; i < env->bindings->length; i++) {
        summa_scheme_value_free(&env->bindings->value[i].value);
    }
    summa_binding_list_free(env->bindings);
    env->bindings = nullptr;
    /* The block itself is ref_count's to free, immediately after this returns. */
}

static void summa_scheme_environment_collect_if_due(void);

SummaSchemeEnvironment summa_scheme_environment_make(SummaSchemeEnvironment parent) {
    return summa_scheme_environment_make_with_capacity(parent, 0);
}

SummaSchemeEnvironment summa_scheme_environment_make_with_capacity(SummaSchemeEnvironment parent, const size_t count) {
    /* Ahead of the allocation rather than after it, so the new environment is
     * never a half-built candidate. Everything a caller is holding at this
     * point is holding a count, which is what makes it a root the pass cannot
     * take away. */
    summa_scheme_environment_collect_if_due();

    const RefCount ref =
        ref_count_make_with_destructor(struct SummaSchemeEnvironment_t, summa_scheme_environment_destroy);
    assert(ref);
    const SummaSchemeEnvironment env = ref_count_as(ref, SummaSchemeEnvironment);

    /* Exactly `count` slots, in one allocation -- and none at all for a frame
     * that binds nothing, which is what a call to a nullary procedure is. */
    env->bindings    = summa_binding_list_make_with_capacity(count);
    env->parent      = summa_scheme_environment_acquire(parent);
    env->trial_count = 0;
    env->reachable   = false;

    env->registry_previous = nullptr;
    env->registry_next     = SUMMA_SCHEME_ENVIRONMENT_REGISTRY;
    if (env->registry_next) {
        env->registry_next->registry_previous = env;
    }
    SUMMA_SCHEME_ENVIRONMENT_REGISTRY = env;
    SUMMA_SCHEME_LIVE_ENVIRONMENTS++;

    return env;
}

void summa_scheme_environment_reserve(const SummaSchemeEnvironment env, const size_t count) {
    summa_binding_list_reserve(env->bindings, count);
}

SummaSchemeEnvironment summa_scheme_environment_make_global(void) {
    SummaSchemeEnvironment env = summa_scheme_environment_make(nullptr);
    summa_scheme_environment_init_global(env);
    return env;
}

void summa_scheme_environment_free(const SummaSchemeEnvironment env) {
    summa_scheme_environment_release(env);
    summa_scheme_collect_cycles();
}

/* ── Trial deletion ────────────────────────────────────────────────────────
 *
 * Counting reclaims everything except a cycle, and closures make cycles the
 * moment a procedure is bound into the frame it closes over -- `letrec`, an
 * internal `define`, a top-level recursive `define`. This is what reclaims
 * those.
 *
 * Mark-and-sweep is not an option here: it needs root enumeration, and the
 * roots are `SummaSchemeEnvironment` values held in C locals across
 * summa_scheme_evaluate_inner, summa_scheme_let, summa_scheme_procedure_frame
 * and summa_scheme_apply, where no collector can see them. Trial deletion needs
 * no roots. It works from the counts:
 *
 *   1. give every live environment a trial count equal to its real one;
 *   2. subtract one for every reference held by another live environment;
 *   3. whatever still has a trial count left is held from outside the registry
 *      -- a C local, or a value in flight -- so it is a root. Mark it, and
 *      everything reachable from it, as live;
 *   4. the unmarked remainder was alive only because it pointed at itself.
 *
 * Step 3 is not optional. A call frame reached only from a root's parent chain
 * has every one of its references subtracted in step 2, and would be collected
 * out from under the evaluator without the marking pass to put it back.
 *
 * Collecting is CPython's clearing step: hold a reference on each condemned
 * environment, break every edge out of the condemned set, then let go. Holding
 * first is what makes the breaking safe in any order. An environment outside
 * the condemned set can lose references here but cannot reach zero -- if the
 * condemned held its last reference, it would not have been marked. */

static void summa_scheme_environment_edge_subtract(SummaSchemeEnvironment* slot, [[maybe_unused]] void* context) {
    if (*slot) {
        /* An edge with no count behind it is edge-set drift -- the enumeration
         * and the counting have stopped agreeing, and the arithmetic below is
         * no longer sound. */
        assert((*slot)->trial_count > 0);
        (*slot)->trial_count--;
    }
}

static void summa_scheme_environment_edge_mark(SummaSchemeEnvironment* slot, void* context) {
    const SummaSchemeEnvironmentList worklist = context;
    if (*slot && !(*slot)->reachable) {
        (*slot)->reachable = true;
        summa_environment_list_push(worklist, slot);
    }
}

size_t summa_scheme_collect_cycles(void) {
    /* A destructor reached from the breaking step must not start a second pass
     * over a registry the first one is in the middle of. */
    if (SUMMA_SCHEME_GC_RUNNING) {
        return 0;
    }
    SUMMA_SCHEME_GC_RUNNING = true;

    for (SummaSchemeEnvironment env = SUMMA_SCHEME_ENVIRONMENT_REGISTRY; env; env = env->registry_next) {
        env->trial_count = summa_scheme_environment_ref(env)->count;
        env->reachable   = false;
    }
    for (SummaSchemeEnvironment env = SUMMA_SCHEME_ENVIRONMENT_REGISTRY; env; env = env->registry_next) {
        summa_scheme_environment_for_each_edge(env, summa_scheme_environment_edge_subtract, nullptr);
    }

    /* Iterated by index rather than popped, so pushes made while marking are
     * picked up and `value` is re-read after a growth reallocates it. */
    const SummaSchemeEnvironmentList worklist = summa_environment_list_make_empty();
    for (SummaSchemeEnvironment env = SUMMA_SCHEME_ENVIRONMENT_REGISTRY; env; env = env->registry_next) {
        if (env->trial_count > 0 && !env->reachable) {
            env->reachable = true;
            summa_environment_list_push(worklist, &env);
        }
    }
    for (size_t i = 0; i < worklist->length; i++) {
        summa_scheme_environment_for_each_edge(worklist->value[i], summa_scheme_environment_edge_mark, worklist);
    }
    summa_environment_list_free(worklist);

    const SummaSchemeEnvironmentList condemned = summa_environment_list_make_empty();
    for (SummaSchemeEnvironment env = SUMMA_SCHEME_ENVIRONMENT_REGISTRY; env; env = env->registry_next) {
        if (!env->reachable) {
            summa_scheme_environment_acquire(env);
            summa_environment_list_push(condemned, &env);
        }
    }
    for (size_t i = 0; i < condemned->length; i++) {
        summa_scheme_environment_for_each_edge(condemned->value[i], summa_scheme_environment_edge_release, nullptr);
    }
    const size_t collected = condemned->length;
    for (size_t i = 0; i < collected; i++) {
        summa_scheme_environment_release(condemned->value[i]);
    }
    summa_environment_list_free(condemned);

    const size_t next         = SUMMA_SCHEME_LIVE_ENVIRONMENTS * 2;
    SUMMA_SCHEME_GC_THRESHOLD = next > SUMMA_SCHEME_GC_MIN_THRESHOLD ? next : (size_t)SUMMA_SCHEME_GC_MIN_THRESHOLD;

    SUMMA_SCHEME_GC_RUNNING = false;
    return collected;
}

static void summa_scheme_environment_collect_if_due(void) {
    if (SUMMA_SCHEME_LIVE_ENVIRONMENTS >= SUMMA_SCHEME_GC_THRESHOLD) {
        summa_scheme_collect_cycles();
    }
}

#pragma endregion Environment lifetime

/* The table lives with the builtins; this only needs the names. */
static size_t                summa_scheme_builtin_count(void);
static SummaSchemeSymbolName summa_scheme_builtin_symbol(size_t index);

void summa_scheme_environment_init_global(SummaSchemeEnvironment env) {
    /* The builtin names are interned by this, and the dispatch below compares
     * against them by pointer. */
    summa_scheme_symbols_ensure();

    /* The one frame whose size is learned after it is made -- the caller hands
     * in an environment and this fills it. Exactly the builtins, in one
     * allocation; anything a program goes on to `define` at the top level grows
     * it the ordinary way. */
    summa_scheme_environment_reserve(env, summa_scheme_builtin_count());

    const SummaSchemeBindingList bindings = env->bindings;
    for (size_t i = 0; i < summa_scheme_builtin_count(); i++) {
        /* The binding's name and the procedure's name are now deliberately the
         * *same* record. They used to be separate strings so that freeing one
         * did not free the other; an interned name is freed by nobody. */
        const SummaSchemeSymbolName name = summa_scheme_builtin_symbol(i);
        summa_binding_list_push(bindings,
                                &summa_scheme_binding_make(
                                    name, summa_make_scheme_procedure(name, summa_symbol_list_make_empty(), nullptr)));
    }
}

SummaSchemeError summa_scheme_environment_set(const SummaSchemeEnvironment env, SummaSchemeBinding newBinding) {
    for (size_t i = 0; i < env->bindings->length; i++) {
        /* By reference: a copy of the element would take the rebinding with it
         * when it went out of scope, leaving the environment untouched. */
        SummaSchemeBinding* binding = &env->bindings->value[i];
        if (binding->name == newBinding.name) {
            /* The old value is being replaced, so it goes before the overwrite
             * rather than after it. The incoming value is moved in rather than
             * copied. The two names are the same interned record -- there is no
             * duplicate left to release, which is what this branch used to have
             * to remember. */
            summa_scheme_value_free(&binding->value);
            binding->value = newBinding.value;
            return summa_success();
        }
    }
    /* Pushed by value: the environment takes ownership of the binding's value
     * from here on. */
    summa_binding_list_push(env->bindings, &newBinding);
    return summa_success();
}

SummaSchemeError summa_scheme_environment_get(const SummaSchemeEnvironment env,
                                              const SummaSchemeSymbol      symbol,
                                              SummaSchemeBinding*          out) {
    return summa_scheme_environment_get_name(env, symbol.value, out);
}

SummaSchemeError summa_scheme_environment_get_name(const SummaSchemeEnvironment env,
                                                   const SummaSchemeSymbolName  name,
                                                   SummaSchemeBinding*          out) {
    SummaSchemeEnvironment owner = nullptr;
    return summa_scheme_environment_get_owner(env, name, out, &owner);
}

SummaSchemeError summa_scheme_environment_get_owner(const SummaSchemeEnvironment env,
                                                    const SummaSchemeSymbolName  name,
                                                    SummaSchemeBinding*          out,
                                                    SummaSchemeEnvironment*      out_owner) {
    /* Iterative rather than recursive over the chain: the owner is what the
     * loop already has to carry, and a deep lexical nesting costs no stack.
     *
     * Still a linear scan per frame -- that is #33's to fix -- but the
     * comparison inside it is now one pointer against another rather than a
     * strcmp, so the walk pays for its length and not for its names. */
    for (SummaSchemeEnvironment scope = env; scope; scope = scope->parent) {
        const SummaSchemeBindingList bindings = scope->bindings;
        for (size_t i = 0; i < bindings->length; i++) {
            if (bindings->value[i].name == name) {
                *out       = bindings->value[i];
                *out_owner = scope;
                return summa_success();
            }
        }
    }
    snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "Unbound variable: %s", name->value);
    return summa_make_error(ERROR_MESSAGE);
}

SummaSchemeError summa_scheme_environment_assign(const SummaSchemeEnvironment env,
                                                 const SummaSchemeSymbolName  name,
                                                 SummaSchemeValue             value) {
    for (size_t i = 0; i < env->bindings->length; i++) {
        /* By reference: a copy would carry the rebinding out of scope. */
        SummaSchemeBinding* binding = &env->bindings->value[i];
        if (binding->name == name) {
            summa_scheme_value_free(&binding->value);
            binding->value = value;
            return summa_success();
        }
    }
    if (env->parent) {
        return summa_scheme_environment_assign(env->parent, name, value);
    }
    /* Nothing took ownership, so `value` is still the caller's to release. */
    snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "set! - unbound variable: %s", name->value);
    return summa_make_error(ERROR_MESSAGE);
}

SummaSchemeError
summa_scheme_evaluate_arguments(const SummaSchemeEnvironment env, const SummaList form, SummaList* out) {
    /* The operator is `form->value[0]`, so the rest of the form is the arity --
     * known before the first argument is evaluated, which is what lets this be
     * one exact allocation rather than the default eight slots plus a doubling
     * for anything wider than that. */
    SummaList args = summa_list_make_with_capacity(form->length > 0 ? form->length - 1 : 0);
    for (size_t i = 1; i < form->length; i++) {
        SummaSchemeValue arg;
        SummaSchemeError err = summa_scheme_evaluate(env, form->value[i], &arg);
        if (err.had) {
            summa_scheme_argument_list_free(args);
            *out = nullptr;
            return err;
        }
        summa_list_push(args, &arg);
    }
    *out = args;
    return summa_success();
}

void summa_scheme_argument_list_free(SummaList args) {
    if (!args) {
        return;
    }
    for (size_t i = 0; i < args->length; i++) {
        summa_scheme_value_free(&args->value[i]);
    }
    summa_list_free(args);
}

static SummaSchemeError summa_scheme_procedure_frame(const SummaSchemeEnvironment env,
                                                     const SummaSchemeProcedure   proc,
                                                     const SummaList              args,
                                                     SummaSchemeEnvironment*      out);

static SummaSchemeError summa_scheme_apply(const SummaSchemeEnvironment env,
                                           const SummaSchemeProcedure   proc,
                                           const SummaList              form,
                                           SummaSchemeValue*            out,
                                           SummaSchemeStep*             step) {
    SummaList        args = nullptr;
    SummaSchemeError err  = summa_scheme_evaluate_arguments(env, form, &args);
    if (!err.had) {
        if (!proc.body) {
            /* A builtin runs to a value here and now -- there is no body to
             * continue into, so nothing to hand back. */
            err = summa_scheme_procedure_dispatch_global(proc, args, out);
        } else {
            SummaSchemeEnvironment frame = nullptr;
            err                          = summa_scheme_procedure_frame(env, proc, args, &frame);
            if (!err.had) {
                /* The body's last expression is a tail position, so the call
                 * ends here rather than nesting: the trampoline picks it up
                 * with the frame this built. `make_closure` guarantees at least
                 * one body expression, so this always yields a tail step. */
                err = summa_scheme_evaluate_sequence_tail(frame, proc.body, 0, out, step);
                /* Only this function's reference -- the step took one of its
                 * own, and so does anything the body defined. */
                summa_scheme_environment_release(frame);
            }
        }
    }
    /* Arguments die with the call; the frame copied what it bound. */
    summa_scheme_argument_list_free(args);
    return err;
}

#pragma region Builtin procedures

/* Arguments already evaluated, one value out. Anything needing an unevaluated
 * operand is a special form, not a builtin. */
typedef SummaSchemeError (*SummaSchemeBuiltinFn)(const SummaList args, SummaSchemeValue* out);

typedef struct {
    const char*          name;
    SummaSchemeBuiltinFn fn;
} SummaSchemeBuiltin;

static SummaSchemeError summa_scheme_require_arity(const char* name, const SummaList args, size_t expected) {
    if (args->length != expected) {
        snprintf(ERROR_MESSAGE,
                 ERROR_MESSAGE_LENGTH,
                 "%s - expects %zu argument(s), got %zu",
                 name,
                 expected,
                 args->length);
        return summa_make_error(ERROR_MESSAGE);
    }
    return summa_success();
}

/* The variadic counterpart. `-` needs one operand to negate, and a chained
 * comparison needs a pair before there is anything to compare. */
static SummaSchemeError summa_scheme_require_min_arity(const char* name, const SummaList args, size_t minimum) {
    if (args->length < minimum) {
        snprintf(ERROR_MESSAGE,
                 ERROR_MESSAGE_LENGTH,
                 "%s - expects at least %zu argument(s), got %zu",
                 name,
                 minimum,
                 args->length);
        return summa_make_error(ERROR_MESSAGE);
    }
    return summa_success();
}

/* For error messages only -- what a user calling the wrong procedure needs to
 * read, not a Scheme-level type name. */
static const char* summa_scheme_type_name(SummaSchemeValueType type) {
    switch (type) {
    case SummaSchemeBooleanType: {
        return "boolean";
    }
    case SummaSchemeCharacterType: {
        return "character";
    }
    case SummaSchemeFloatingType: {
        return "floating";
    }
    case SummaSchemeIntegerType: {
        return "integer";
    }
    case SummaSchemeListType: {
        return "list";
    }
    case SummaSchemeProcedureType: {
        return "procedure";
    }
    case SummaSchemeStringType: {
        return "string";
    }
    case SummaSchemeSymbolType: {
        return "symbol";
    }
    case SummaSchemeVectorType: {
        return "vector";
    }
    }
    return "value";
}

/* The position is named as well as the type, so a two-argument builtin says
 * which operand was wrong. One-based, the way a caller counts them. */
static SummaSchemeError
summa_scheme_require_type(const char* name, const SummaList args, size_t index, SummaSchemeValueType expected) {
    if (args->value[index].type != expected) {
        snprintf(ERROR_MESSAGE,
                 ERROR_MESSAGE_LENGTH,
                 "%s - argument %zu must be %s, got %s",
                 name,
                 index + 1,
                 summa_scheme_type_name(expected),
                 summa_scheme_type_name(args->value[index].type));
        return summa_make_error(ERROR_MESSAGE);
    }
    return summa_success();
}

/* The common shape for a fixed-arity builtin whose operands are all one type:
 * count them, then check each. Saves every such builtin repeating both. */
static SummaSchemeError summa_scheme_require_arity_of_type(const char*          name,
                                                           const SummaList      args,
                                                           size_t               expected,
                                                           SummaSchemeValueType type) {
    const SummaSchemeError err = summa_scheme_require_arity(name, args, expected);
    if (err.had) {
        return err;
    }
    for (size_t i = 0; i < expected; i++) {
        const SummaSchemeError type_err = summa_scheme_require_type(name, args, i, type);
        if (type_err.had) {
            return type_err;
        }
    }
    return summa_success();
}

static bool summa_scheme_is_number(const SummaSchemeValue* value) {
    return value->type == SummaSchemeIntegerType || value->type == SummaSchemeFloatingType;
}

static double summa_scheme_number_to_double(const SummaSchemeValue* value) {
    return value->type == SummaSchemeFloatingType ? value->value.floating.value : (double)value->value.integer.value;
}

static SummaSchemeError summa_scheme_require_numbers(const char* name, const SummaList args) {
    for (size_t i = 0; i < args->length; i++) {
        if (!summa_scheme_is_number(&args->value[i])) {
            snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - expects numeric arguments", name);
            return summa_make_error(ERROR_MESSAGE);
        }
    }
    return summa_success();
}

/* One inexact operand makes the result inexact. */
static bool summa_scheme_has_floating(const SummaList args) {
    for (size_t i = 0; i < args->length; i++) {
        if (args->value[i].type == SummaSchemeFloatingType) {
            return true;
        }
    }
    return false;
}

static SummaSchemeError summa_scheme_builtin_add(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_numbers("+", args);
    if (err.had) {
        return err;
    }

    if (summa_scheme_has_floating(args)) {
        double result = 0.0;
        for (size_t i = 0; i < args->length; i++) {
            result += summa_scheme_number_to_double(&args->value[i]);
        }
        *out = summa_make_scheme_floating(result);
    } else {
        int64_t result = 0;
        for (size_t i = 0; i < args->length; i++) {
            result += args->value[i].value.integer.value;
        }
        *out = summa_make_scheme_integer(result);
    }
    return summa_success();
}

/* One argument negates; two or more fold left. Zero would have no identity to
 * return -- R7RS makes `(-)` an error, and so does this. */
static SummaSchemeError summa_scheme_builtin_subtract(const SummaList args, SummaSchemeValue* out) {
    SummaSchemeError err = summa_scheme_require_min_arity("-", args, 1);
    if (err.had) {
        return err;
    }
    err = summa_scheme_require_numbers("-", args);
    if (err.had) {
        return err;
    }

    if (summa_scheme_has_floating(args)) {
        double result = summa_scheme_number_to_double(&args->value[0]);
        if (args->length == 1) {
            result = -result;
        }
        for (size_t i = 1; i < args->length; i++) {
            result -= summa_scheme_number_to_double(&args->value[i]);
        }
        *out = summa_make_scheme_floating(result);
    } else {
        int64_t result = args->value[0].value.integer.value;
        if (args->length == 1) {
            result = -result;
        }
        for (size_t i = 1; i < args->length; i++) {
            result -= args->value[i].value.integer.value;
        }
        *out = summa_make_scheme_integer(result);
    }
    return summa_success();
}

static SummaSchemeError summa_scheme_builtin_multiply(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_numbers("*", args);
    if (err.had) {
        return err;
    }

    if (summa_scheme_has_floating(args)) {
        double result = 1.0;
        for (size_t i = 0; i < args->length; i++) {
            result *= summa_scheme_number_to_double(&args->value[i]);
        }
        *out = summa_make_scheme_floating(result);
    } else {
        int64_t result = 1;
        for (size_t i = 0; i < args->length; i++) {
            result *= args->value[i].value.integer.value;
        }
        *out = summa_make_scheme_integer(result);
    }
    return summa_success();
}

/* -1, 0 or 1. Two integers are compared as integers rather than through
 * `double`, so magnitudes past 2^53 still order correctly. */
static int summa_scheme_number_compare(const SummaSchemeValue* left, const SummaSchemeValue* right) {
    if (left->type == SummaSchemeIntegerType && right->type == SummaSchemeIntegerType) {
        const int64_t a = left->value.integer.value;
        const int64_t b = right->value.integer.value;
        return a < b ? -1 : (a > b ? 1 : 0);
    }
    const double a = summa_scheme_number_to_double(left);
    const double b = summa_scheme_number_to_double(right);
    return a < b ? -1 : (a > b ? 1 : 0);
}

/* Decides one adjacent pair from its ordering -- the only thing the five
 * comparisons differ by. */
typedef bool (*SummaSchemeOrderFn)(int ordering);

static bool summa_scheme_order_equal(int ordering) {
    return ordering == 0;
}

static bool summa_scheme_order_less(int ordering) {
    return ordering < 0;
}

static bool summa_scheme_order_greater(int ordering) {
    return ordering > 0;
}

static bool summa_scheme_order_less_equal(int ordering) {
    return ordering <= 0;
}

static bool summa_scheme_order_greater_equal(int ordering) {
    return ordering >= 0;
}

/* R7RS chaining: every adjacent pair must hold, so `(< 1 2 3)` is #t. Stops at
 * the first pair that fails. Two operands minimum -- a one-operand comparison
 * is vacuously true and almost certainly a mistake. */
static SummaSchemeError
summa_scheme_compare_chain(const char* name, const SummaList args, SummaSchemeOrderFn accept, SummaSchemeValue* out) {
    SummaSchemeError err = summa_scheme_require_min_arity(name, args, 2);
    if (err.had) {
        return err;
    }
    err = summa_scheme_require_numbers(name, args);
    if (err.had) {
        return err;
    }

    for (size_t i = 1; i < args->length; i++) {
        if (!accept(summa_scheme_number_compare(&args->value[i - 1], &args->value[i]))) {
            *out = summa_make_scheme_boolean(false);
            return summa_success();
        }
    }
    *out = summa_make_scheme_boolean(true);
    return summa_success();
}

static SummaSchemeError summa_scheme_builtin_numeric_equal(const SummaList args, SummaSchemeValue* out) {
    return summa_scheme_compare_chain("=", args, summa_scheme_order_equal, out);
}

static SummaSchemeError summa_scheme_builtin_less(const SummaList args, SummaSchemeValue* out) {
    return summa_scheme_compare_chain("<", args, summa_scheme_order_less, out);
}

static SummaSchemeError summa_scheme_builtin_greater(const SummaList args, SummaSchemeValue* out) {
    return summa_scheme_compare_chain(">", args, summa_scheme_order_greater, out);
}

static SummaSchemeError summa_scheme_builtin_less_equal(const SummaList args, SummaSchemeValue* out) {
    return summa_scheme_compare_chain("<=", args, summa_scheme_order_less_equal, out);
}

static SummaSchemeError summa_scheme_builtin_greater_equal(const SummaList args, SummaSchemeValue* out) {
    return summa_scheme_compare_chain(">=", args, summa_scheme_order_greater_equal, out);
}

/* Shared front end for `quotient`, `remainder` and `modulo`: two integers, a
 * nonzero divisor, and a representable result.
 *
 * R7RS admits integral floats here -- `(modulo 7.0 2)` is 1.0 -- but
 * SummaSchemeValue tracks no exactness, so an integral float is
 * indistinguishable from one that merely rounded to look integral. Nothing in
 * the euler suites wants it, so a float is rejected outright rather than
 * silently truncated. */
static SummaSchemeError
summa_scheme_integer_operands(const char* name, const SummaList args, int64_t* left, int64_t* right) {
    const SummaSchemeError err = summa_scheme_require_arity_of_type(name, args, 2, SummaSchemeIntegerType);
    if (err.had) {
        return err;
    }

    const int64_t dividend = args->value[0].value.integer.value;
    const int64_t divisor  = args->value[1].value.integer.value;
    if (divisor == 0) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - division by zero", name);
        return summa_make_error(ERROR_MESSAGE);
    }
    /* INT64_MIN / -1 is +2^63, which does not fit an int64_t. C leaves both
     * `/` and `%` undefined for it rather than wrapping, and UBSan traps it in
     * CI -- so it is an error here, not a value. */
    if (dividend == INT64_MIN && divisor == -1) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - result is not representable", name);
        return summa_make_error(ERROR_MESSAGE);
    }

    *left  = dividend;
    *right = divisor;
    return summa_success();
}

static SummaSchemeError summa_scheme_builtin_quotient(const SummaList args, SummaSchemeValue* out) {
    int64_t                left  = 0;
    int64_t                right = 0;
    const SummaSchemeError err   = summa_scheme_integer_operands("quotient", args, &left, &right);
    if (err.had) {
        return err;
    }
    *out = summa_make_scheme_integer(left / right);
    return summa_success();
}

/* C's `%` truncates toward zero, which is exactly `remainder`: the sign of the
 * result follows the dividend. */
static SummaSchemeError summa_scheme_builtin_remainder(const SummaList args, SummaSchemeValue* out) {
    int64_t                left  = 0;
    int64_t                right = 0;
    const SummaSchemeError err   = summa_scheme_integer_operands("remainder", args, &left, &right);
    if (err.had) {
        return err;
    }
    *out = summa_make_scheme_integer(left % right);
    return summa_success();
}

/* `modulo` takes the sign of the divisor instead, so a remainder that
 * disagrees with it is pulled one divisor across. The correction cannot
 * overflow: the remainder is smaller in magnitude than the divisor and points
 * the other way, so the sum lies strictly between zero and the divisor. */
static SummaSchemeError summa_scheme_builtin_modulo(const SummaList args, SummaSchemeValue* out) {
    int64_t                left  = 0;
    int64_t                right = 0;
    const SummaSchemeError err   = summa_scheme_integer_operands("modulo", args, &left, &right);
    if (err.had) {
        return err;
    }
    int64_t result = left % right;
    if (result != 0 && ((result < 0) != (right < 0))) {
        result += right;
    }
    *out = summa_make_scheme_integer(result);
    return summa_success();
}

static SummaSchemeError summa_scheme_builtin_is_zero(const SummaList args, SummaSchemeValue* out) {
    SummaSchemeError err = summa_scheme_require_arity("zero?", args, 1);
    if (err.had) {
        return err;
    }
    err = summa_scheme_require_numbers("zero?", args);
    if (err.had) {
        return err;
    }

    const SummaSchemeValue* value = &args->value[0];
    const bool              zero =
        value->type == SummaSchemeIntegerType ? value->value.integer.value == 0 : value->value.floating.value == 0.0;
    *out = summa_make_scheme_boolean(zero);
    return summa_success();
}

/* The one place `#f` is singled out: everything else is true, so `not` gives
 * back #f for it. */
static SummaSchemeError summa_scheme_builtin_not(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_arity("not", args, 1);
    if (err.had) {
        return err;
    }
    *out = summa_make_scheme_boolean(!summa_scheme_truthy(&args->value[0]));
    return summa_success();
}

static SummaSchemeError summa_scheme_builtin_string_length(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_arity_of_type("string-length", args, 1, SummaSchemeStringType);
    if (err.had) {
        return err;
    }
    *out = summa_make_scheme_integer((int64_t)args->value[0].value.string.value->length);
    return summa_success();
}

static SummaSchemeError summa_scheme_builtin_string_ref(const SummaList args, SummaSchemeValue* out) {
    SummaSchemeError err = summa_scheme_require_arity("string-ref", args, 2);
    if (err.had) {
        return err;
    }
    err = summa_scheme_require_type("string-ref", args, 0, SummaSchemeStringType);
    if (err.had) {
        return err;
    }
    err = summa_scheme_require_type("string-ref", args, 1, SummaSchemeIntegerType);
    if (err.had) {
        return err;
    }

    const SummaString str   = args->value[0].value.string.value;
    const int64_t     index = args->value[1].value.integer.value;
    /* Both bounds in one message: which index was asked for, and what the
     * string could have answered. */
    if (index < 0 || (size_t)index >= str->length) {
        snprintf(ERROR_MESSAGE,
                 ERROR_MESSAGE_LENGTH,
                 "string-ref - index %" PRId64 " is out of range for a string of length %zu",
                 index,
                 str->length);
        return summa_make_error(ERROR_MESSAGE);
    }
    *out = summa_make_scheme_character(str->value[index]);
    return summa_success();
}

static SummaSchemeError summa_scheme_builtin_char_to_integer(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err =
        summa_scheme_require_arity_of_type("char->integer", args, 1, SummaSchemeCharacterType);
    if (err.had) {
        return err;
    }
    /* Through `unsigned char`: plain `char` is signed on most targets, and a
     * byte past 127 must not come back as a negative code point. */
    *out = summa_make_scheme_integer((unsigned char)args->value[0].value.character.value);
    return summa_success();
}

/* A list value carries a null handle as readily as a zero-length one -- the
 * reader only ever builds the second, but a value assembled in C can hold the
 * first -- so both are the empty list here rather than one of them a segfault. */
static size_t summa_scheme_list_length(const SummaSchemeValue* value) {
    return value->value.list.value ? value->value.list.value->length : 0;
}

/* What `car` and `cdr` both want: one operand, a list, and not the empty one.
 * `(car '())` has no element to hand back, so it is an error rather than a
 * value -- unlike `(cdr '(1))`, whose tail is legitimately `()`. */
static SummaSchemeError summa_scheme_require_nonempty_list(const char* name, const SummaList args) {
    const SummaSchemeError err = summa_scheme_require_arity_of_type(name, args, 1, SummaSchemeListType);
    if (err.had) {
        return err;
    }
    if (summa_scheme_list_length(&args->value[0]) == 0) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - expects a non-empty list", name);
        return summa_make_error(ERROR_MESSAGE);
    }
    return summa_success();
}

/* Lists are dynamic arrays rather than cons cells, so the tail has to be a
 * list: `(cons 1 2)` is an improper pair and has no representation here.
 * Coercing it to `(1 2)` would be a different answer wearing the same name.
 *
 * Prepending is therefore a new list of length n+1, not a cell pointing at the
 * old one -- which is also what makes the result outlive the arguments. */
static SummaSchemeError summa_scheme_builtin_cons(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_arity("cons", args, 2);
    if (err.had) {
        return err;
    }
    if (args->value[1].type != SummaSchemeListType) {
        snprintf(ERROR_MESSAGE,
                 ERROR_MESSAGE_LENGTH,
                 "cons - argument 2 must be list, got %s (there are no improper pairs)",
                 summa_scheme_type_name(args->value[1].type));
        return summa_make_error(ERROR_MESSAGE);
    }

    const SummaList  tail  = args->value[1].value.list.value;
    const SummaList  items = summa_list_make_empty();
    SummaSchemeValue head;
    summa_scheme_value_copy(&head, &args->value[0]);
    summa_list_push(items, &head);
    for (size_t i = 0; tail && i < tail->length; i++) {
        SummaSchemeValue element;
        summa_scheme_value_copy(&element, &tail->value[i]);
        summa_list_push(items, &element);
    }
    *out = summa_make_scheme_list(items);
    return summa_success();
}

static SummaSchemeError summa_scheme_builtin_car(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_nonempty_list("car", args);
    if (err.had) {
        return err;
    }
    /* A copy, never a pointer into the argument: `summa_scheme_apply` releases
     * the argument list the instant dispatch returns. */
    return summa_scheme_value_copy(out, &args->value[0].value.list.value->value[0]);
}

/* The tail copied element by element, so no two values ever observe the same
 * one. `(cdr '(1))` is `()` -- a list with nothing left in it, which is a
 * value, not the error `(car '())` is. */
static SummaSchemeError summa_scheme_builtin_cdr(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_nonempty_list("cdr", args);
    if (err.had) {
        return err;
    }

    const SummaList source = args->value[0].value.list.value;
    const SummaList items  = summa_list_make_empty();
    for (size_t i = 1; i < source->length; i++) {
        SummaSchemeValue element;
        summa_scheme_value_copy(&element, &source->value[i]);
        summa_list_push(items, &element);
    }
    *out = summa_make_scheme_list(items);
    return summa_success();
}

/* Variadic all the way down: `(list)` is `()`. Every operand is deep-copied,
 * so a list built out of other lists owns its elements outright. */
static SummaSchemeError summa_scheme_builtin_list(const SummaList args, SummaSchemeValue* out) {
    *out = summa_make_scheme_list(summa_scheme_list_copy_deep(args));
    return summa_success();
}

/* The empty list alone. `#()` is an empty *vector* -- a distinct type, and not
 * null, which is the distinction a length check on its own would lose. */
static SummaSchemeError summa_scheme_builtin_is_null(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_arity("null?", args, 1);
    if (err.had) {
        return err;
    }
    const SummaSchemeValue* value = &args->value[0];
    *out = summa_make_scheme_boolean(value->type == SummaSchemeListType && summa_scheme_list_length(value) == 0);
    return summa_success();
}

/* Anything with a car, which without cons cells means a list of at least one
 * element. The exact complement of `null?` over lists, and #f for every other
 * type -- neither predicate is an error on one. */
static SummaSchemeError summa_scheme_builtin_is_pair(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_arity("pair?", args, 1);
    if (err.had) {
        return err;
    }
    const SummaSchemeValue* value = &args->value[0];
    *out = summa_make_scheme_boolean(value->type == SummaSchemeListType && summa_scheme_list_length(value) > 0);
    return summa_success();
}

/* The first of the type predicates. It is here rather than with the rest of
 * that family because environment lifetime needs it: a closure stored into the
 * frame it captured can only be checked from Scheme by asking what came back
 * out. */
static SummaSchemeError summa_scheme_builtin_is_procedure(const SummaList args, SummaSchemeValue* out) {
    const SummaSchemeError err = summa_scheme_require_arity("procedure?", args, 1);
    if (err.had) {
        return err;
    }
    *out = summa_make_scheme_boolean(args->value[0].type == SummaSchemeProcedureType);
    return summa_success();
}

/* One function plus one row: the global environment binds a procedure per row
 * at startup, and dispatch finds it by name. */
static const SummaSchemeBuiltin SUMMA_SCHEME_BUILTINS[] = {
    {"+", summa_scheme_builtin_add},
    {"-", summa_scheme_builtin_subtract},
    {"*", summa_scheme_builtin_multiply},
    {"=", summa_scheme_builtin_numeric_equal},
    {"<", summa_scheme_builtin_less},
    {">", summa_scheme_builtin_greater},
    {"<=", summa_scheme_builtin_less_equal},
    {">=", summa_scheme_builtin_greater_equal},
    {"quotient", summa_scheme_builtin_quotient},
    {"remainder", summa_scheme_builtin_remainder},
    {"modulo", summa_scheme_builtin_modulo},
    {"zero?", summa_scheme_builtin_is_zero},
    {"not", summa_scheme_builtin_not},
    {"string-length", summa_scheme_builtin_string_length},
    {"string-ref", summa_scheme_builtin_string_ref},
    {"char->integer", summa_scheme_builtin_char_to_integer},
    {"cons", summa_scheme_builtin_cons},
    {"car", summa_scheme_builtin_car},
    {"cdr", summa_scheme_builtin_cdr},
    {"list", summa_scheme_builtin_list},
    {"null?", summa_scheme_builtin_is_null},
    {"pair?", summa_scheme_builtin_is_pair},
    {"procedure?", summa_scheme_builtin_is_procedure},
};

#define SUMMA_SCHEME_BUILTIN_COUNT (sizeof(SUMMA_SCHEME_BUILTINS) / sizeof(SUMMA_SCHEME_BUILTINS[0]))

/* Row i's name, interned. Filled in by summa_scheme_symbols_ensure, which is
 * what turns the dispatch below from twenty-three strcmps per builtin call into
 * twenty-three pointer compares. */
static SummaSchemeSymbolName SUMMA_SCHEME_BUILTIN_SYMBOLS[SUMMA_SCHEME_BUILTIN_COUNT];

static size_t summa_scheme_builtin_count(void) {
    return SUMMA_SCHEME_BUILTIN_COUNT;
}

static SummaSchemeSymbolName summa_scheme_builtin_symbol(size_t index) {
    return SUMMA_SCHEME_BUILTIN_SYMBOLS[index];
}

SummaSchemeError
summa_scheme_procedure_dispatch_global(SummaSchemeProcedure proc, const SummaList args, SummaSchemeValue* out) {
    for (size_t i = 0; i < SUMMA_SCHEME_BUILTIN_COUNT; i++) {
        if (proc.name == SUMMA_SCHEME_BUILTIN_SYMBOLS[i]) {
            return SUMMA_SCHEME_BUILTINS[i].fn(args, out);
        }
    }
    snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "Unknown builtin procedure: %s", proc.name->value);
    return summa_make_error(ERROR_MESSAGE);
}

#pragma endregion Builtin procedures

#pragma region Special forms

/* Everything but the last expression is evaluated here, for effect; the last
 * one is the tail position and goes back to the trampoline. An empty range is
 * not an error -- it is the unspecified value, and no tail step at all. */
static SummaSchemeError summa_scheme_evaluate_sequence_tail(const SummaSchemeEnvironment env,
                                                            const SummaList              form,
                                                            size_t                       start,
                                                            SummaSchemeValue*            out,
                                                            SummaSchemeStep*             step) {
    if (start >= form->length) {
        *out = summa_scheme_unspecified();
        return summa_success();
    }

    for (size_t i = start; i + 1 < form->length; i++) {
        SummaSchemeValue       next;
        const SummaSchemeError err = summa_scheme_evaluate(env, form->value[i], &next);
        if (err.had) {
            return err;
        }
        /* Evaluated only for its effect: a body expression that is not the last
         * one has nowhere for its value to go. */
        summa_scheme_value_free(&next);
    }

    summa_scheme_step_set_tail(step, &form->value[form->length - 1], env);
    return summa_success();
}

static SummaSchemeError summa_scheme_evaluate_sequence(const SummaSchemeEnvironment env,
                                                       const SummaList              form,
                                                       size_t                       start,
                                                       SummaSchemeValue*            out) {
    SummaSchemeStep  step = summa_scheme_step_value();
    SummaSchemeError err  = summa_scheme_evaluate_sequence_tail(env, form, start, out, &step);
    if (!err.had && step.kind == SummaSchemeStepTail) {
        err = summa_scheme_evaluate(step.environment, *step.expression, out);
    }
    summa_scheme_step_release(&step);
    return err;
}

static SummaSchemeError summa_scheme_special_quote([[maybe_unused]] const SummaSchemeEnvironment env,
                                                   const SummaList                               form,
                                                   SummaSchemeValue*                             out,
                                                   [[maybe_unused]] SummaSchemeStep*             step) {
    if (form->length != 2) {
        return summa_make_error("quote - expects exactly one operand");
    }
    /* The point of the form: copied, never evaluated. */
    return summa_scheme_value_copy(out, &form->value[1]);
}

static SummaSchemeError summa_scheme_special_if(const SummaSchemeEnvironment env,
                                                const SummaList              form,
                                                SummaSchemeValue*            out,
                                                SummaSchemeStep*             step) {
    if (form->length < 3 || form->length > 4) {
        return summa_make_error("if - expects (if test consequent [alternate])");
    }

    /* The test is not a tail position -- its value decides something here, so
     * this one recurses. */
    SummaSchemeValue       test;
    const SummaSchemeError err = summa_scheme_evaluate(env, form->value[1], &test);
    if (err.had) {
        return err;
    }
    const bool truthy = summa_scheme_truthy(&test);
    summa_scheme_value_free(&test);

    /* Exactly one branch, and it is a tail position: handed back rather than
     * evaluated, which is what makes `(if done acc (loop (- n 1) ...))` a loop
     * rather than a stack. Recursion terminating rests on the first half of
     * that; recursion being unbounded rests on the second. */
    if (truthy) {
        summa_scheme_step_set_tail(step, &form->value[2], env);
        return summa_success();
    }
    if (form->length == 4) {
        summa_scheme_step_set_tail(step, &form->value[3], env);
        return summa_success();
    }
    *out = summa_scheme_unspecified();
    return summa_success();
}

/* Parameter names and body copied out; defining environment captured by
 * handle. `params` is only read -- its elements stay the caller's. */
static SummaSchemeError summa_scheme_make_closure(const SummaSchemeEnvironment env,
                                                  const SummaSchemeSymbolName  name,
                                                  const SummaList              params,
                                                  const SummaList              form,
                                                  size_t                       body_start,
                                                  SummaSchemeValue*            out) {
    if (form->length <= body_start) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - expects at least one body expression", name->value);
        return summa_make_error(ERROR_MESSAGE);
    }

    SummaSchemeSymbolList bindings = summa_symbol_list_make_empty();
    for (size_t i = 0; params && i < params->length; i++) {
        if (params->value[i].type != SummaSchemeSymbolType) {
            summa_symbol_list_free(bindings);
            snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - parameter names must be symbols", name->value);
            return summa_make_error(ERROR_MESSAGE);
        }
        /* The parameter name is already interned -- it came out of the reader
         * that way -- so this is the pointer, not a copy of the characters. */
        summa_symbol_list_push(bindings, &params->value[i].value.symbol);
    }

    SummaList body = summa_list_make_empty();
    for (size_t i = body_start; i < form->length; i++) {
        SummaSchemeValue expression;
        summa_scheme_value_copy(&expression, &form->value[i]);
        summa_list_push(body, &expression);
    }

    /* The capture is a reference, not a borrow: the procedure now holds its
     * defining environment up, which is what lets it be called after the frame
     * it was written in has returned. */
    *out = summa_make_scheme_closure(name, bindings, body, summa_scheme_environment_acquire(env));
    return summa_success();
}

static SummaSchemeError summa_scheme_special_lambda(const SummaSchemeEnvironment      env,
                                                    const SummaList                   form,
                                                    SummaSchemeValue*                 out,
                                                    [[maybe_unused]] SummaSchemeStep* step) {
    if (form->length < 3) {
        return summa_make_error("lambda - expects (lambda (parameters ...) body ...)");
    }
    if (form->value[1].type != SummaSchemeListType) {
        return summa_make_error("lambda - parameter list must be a list");
    }
    return summa_scheme_make_closure(env, SUMMA_SCHEME_SYMBOL_LAMBDA, form->value[1].value.list.value, form, 2, out);
}

/* `define` has no tail position: the value expression's result is bound rather
 * than returned, so there is still work to do after it. */
static SummaSchemeError summa_scheme_special_define(const SummaSchemeEnvironment      env,
                                                    const SummaList                   form,
                                                    SummaSchemeValue*                 out,
                                                    [[maybe_unused]] SummaSchemeStep* step) {
    if (form->length < 3) {
        return summa_make_error("define - expects (define name value) or (define (name parameters ...) body ...)");
    }

    const SummaSchemeValue target = form->value[1];

    /* (define (name parameters ...) body ...) -- the procedure shorthand. */
    if (target.type == SummaSchemeListType) {
        const SummaList signature = target.value.list.value;
        if (!signature || signature->length == 0 || signature->value[0].type != SummaSchemeSymbolType) {
            return summa_make_error("define - procedure name must be a symbol");
        }
        const SummaSchemeSymbolName name = signature->value[0].value.symbol.value;

        /* The signature minus its head. Elements are borrowed, so this list is
         * released shallowly. */
        SummaList params = summa_list_make_empty();
        for (size_t i = 1; i < signature->length; i++) {
            summa_list_push(params, &signature->value[i]);
        }
        SummaSchemeValue       procedure;
        const SummaSchemeError err = summa_scheme_make_closure(env, name, params, form, 2, &procedure);
        summa_list_free(params);
        if (err.had) {
            return err;
        }
        summa_scheme_environment_set(env, summa_scheme_binding_make(name, procedure));
        *out = summa_make_scheme_symbol_name(name);
        return summa_success();
    }

    if (target.type != SummaSchemeSymbolType) {
        return summa_make_error("define - name must be a symbol");
    }
    if (form->length != 3) {
        return summa_make_error("define - expects exactly one value expression");
    }

    SummaSchemeValue       value;
    const SummaSchemeError err = summa_scheme_evaluate(env, form->value[2], &value);
    if (err.had) {
        return err;
    }
    /* One interned record, named by both the binding and the symbol handed
     * back. Where this needed three separate strings with three owners, there
     * is now nothing here to own. */
    const SummaSchemeSymbolName name = target.value.symbol.value;
    summa_scheme_environment_set(env, summa_scheme_binding_make(name, value));
    *out = summa_make_scheme_symbol_name(name);
    return summa_success();
}

static SummaSchemeError summa_scheme_special_set(const SummaSchemeEnvironment      env,
                                                 const SummaList                   form,
                                                 SummaSchemeValue*                 out,
                                                 [[maybe_unused]] SummaSchemeStep* step) {
    if (form->length != 3) {
        return summa_make_error("set! - expects (set! name value)");
    }
    if (form->value[1].type != SummaSchemeSymbolType) {
        return summa_make_error("set! - name must be a symbol");
    }

    SummaSchemeValue value;
    SummaSchemeError err = summa_scheme_evaluate(env, form->value[2], &value);
    if (err.had) {
        return err;
    }

    /* assign, not set: mutate where the chain holds it, do not shadow. */
    err = summa_scheme_environment_assign(env, form->value[1].value.symbol.value, value);
    if (err.had) {
        /* Rejected, so ownership never transferred. */
        summa_scheme_value_free(&value);
        return err;
    }
    *out = summa_scheme_unspecified();
    return summa_success();
}

static SummaSchemeError summa_scheme_special_begin(const SummaSchemeEnvironment env,
                                                   const SummaList              form,
                                                   SummaSchemeValue*            out,
                                                   SummaSchemeStep*             step) {
    return summa_scheme_evaluate_sequence_tail(env, form, 1, out, step);
}

/* The three flavours differ only in which environment the initializers see. */
typedef enum {
    /* `let`: initializers see the outer environment -- bindings are simultaneous. */
    SUMMA_SCHEME_LET_PARALLEL,
    /* `let*`: each initializer sees the bindings made before it. */
    SUMMA_SCHEME_LET_SEQUENTIAL,
    /* `letrec`: every name bound before any initializer runs, so the
     * procedures defined can refer to each other. */
    SUMMA_SCHEME_LET_RECURSIVE,
} SummaSchemeLetKind;

static SummaSchemeError summa_scheme_let_clause(const char*              name,
                                                const SummaSchemeValue*  clause,
                                                SummaSchemeSymbolName*   out_name,
                                                const SummaSchemeValue** out_expression) {
    if (clause->type != SummaSchemeListType || !clause->value.list.value || clause->value.list.value->length != 2 ||
        clause->value.list.value->value[0].type != SummaSchemeSymbolType) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - each binding must be (name value)", name);
        return summa_make_error(ERROR_MESSAGE);
    }
    *out_name       = clause->value.list.value->value[0].value.symbol.value;
    *out_expression = &clause->value.list.value->value[1];
    return summa_success();
}

static SummaSchemeError summa_scheme_let(const char*                  name,
                                         const SummaSchemeLetKind     kind,
                                         const SummaSchemeEnvironment env,
                                         const SummaList              form,
                                         SummaSchemeValue*            out,
                                         SummaSchemeStep*             step) {
    if (form->length < 3) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - expects (%s ((name value) ...) body ...)", name, name);
        return summa_make_error(ERROR_MESSAGE);
    }
    if (form->value[1].type != SummaSchemeListType) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - bindings must be a list", name);
        return summa_make_error(ERROR_MESSAGE);
    }

    /* One clause is one binding on every one of the three paths -- `letrec`
     * binds each name twice, but the second is a rebind of the first rather
     * than a push. */
    const SummaList              clauses = form->value[1].value.list.value;
    const SummaSchemeEnvironment frame =
        summa_scheme_environment_make_with_capacity(env, clauses ? clauses->length : 0);
    SummaSchemeError err = summa_success();

    if (kind == SUMMA_SCHEME_LET_RECURSIVE) {
        /* Names first, values second -- the gap is the point of letrec. */
        for (size_t i = 0; clauses && i < clauses->length; i++) {
            SummaSchemeSymbolName   binding_name = nullptr;
            const SummaSchemeValue* expression   = nullptr;
            err = summa_scheme_let_clause(name, &clauses->value[i], &binding_name, &expression);
            if (err.had) {
                break;
            }
            summa_scheme_environment_set(frame, summa_scheme_binding_make(binding_name, summa_scheme_unspecified()));
        }
    }

    for (size_t i = 0; !err.had && clauses && i < clauses->length; i++) {
        SummaSchemeSymbolName   binding_name = nullptr;
        const SummaSchemeValue* expression   = nullptr;
        err = summa_scheme_let_clause(name, &clauses->value[i], &binding_name, &expression);
        if (err.had) {
            break;
        }

        /* Parallel evaluates against the outer environment, so an initializer
         * cannot see its siblings; the other two use the frame. */
        SummaSchemeValue value;
        err = summa_scheme_evaluate(kind == SUMMA_SCHEME_LET_PARALLEL ? env : frame, *expression, &value);
        if (err.had) {
            break;
        }
        summa_scheme_environment_set(frame, summa_scheme_binding_make(binding_name, value));
    }

    if (!err.had) {
        /* The last body expression is a tail position, evaluated in the frame.
         * The step acquires its own reference to it, which is what lets the
         * release below happen before the body has run. */
        err = summa_scheme_evaluate_sequence_tail(frame, form, 2, out, step);
    }
    /* Only this function's own reference. A lambda made in the body kept one of
     * its own, so `(let ((n 7)) (lambda () n))` outlives the form; a letrec
     * bound into its own frame kept one that points back here, which is the
     * cycle the collector exists for. */
    summa_scheme_environment_release(frame);
    return err;
}

static SummaSchemeError summa_scheme_special_let(const SummaSchemeEnvironment env,
                                                 const SummaList              form,
                                                 SummaSchemeValue*            out,
                                                 SummaSchemeStep*             step) {
    return summa_scheme_let("let", SUMMA_SCHEME_LET_PARALLEL, env, form, out, step);
}

static SummaSchemeError summa_scheme_special_let_star(const SummaSchemeEnvironment env,
                                                      const SummaList              form,
                                                      SummaSchemeValue*            out,
                                                      SummaSchemeStep*             step) {
    return summa_scheme_let("let*", SUMMA_SCHEME_LET_SEQUENTIAL, env, form, out, step);
}

static SummaSchemeError summa_scheme_special_letrec(const SummaSchemeEnvironment env,
                                                    const SummaList              form,
                                                    SummaSchemeValue*            out,
                                                    SummaSchemeStep*             step) {
    return summa_scheme_let("letrec", SUMMA_SCHEME_LET_RECURSIVE, env, form, out, step);
}

static SummaSchemeError summa_scheme_special_cond(const SummaSchemeEnvironment env,
                                                  const SummaList              form,
                                                  SummaSchemeValue*            out,
                                                  SummaSchemeStep*             step) {
    for (size_t i = 1; i < form->length; i++) {
        const SummaSchemeValue clause = form->value[i];
        if (clause.type != SummaSchemeListType || !clause.value.list.value || clause.value.list.value->length == 0) {
            return summa_make_error("cond - each clause must be (test expression ...)");
        }
        const SummaList body = clause.value.list.value;

        const bool is_else = body->value[0].type == SummaSchemeSymbolType &&
                             body->value[0].value.symbol.value == SUMMA_SCHEME_SYMBOL_ELSE;
        if (is_else) {
            return summa_scheme_evaluate_sequence_tail(env, body, 1, out, step);
        }

        /* The test decides which clause runs, so it is not a tail position. */
        SummaSchemeValue       test;
        const SummaSchemeError err = summa_scheme_evaluate(env, body->value[0], &test);
        if (err.had) {
            return err;
        }
        if (!summa_scheme_truthy(&test)) {
            summa_scheme_value_free(&test);
            continue;
        }
        /* A clause that is nothing but a test yields the test's own value. */
        if (body->length == 1) {
            *out = test;
            return summa_success();
        }
        summa_scheme_value_free(&test);
        /* The selected clause's last expression is the tail position. */
        return summa_scheme_evaluate_sequence_tail(env, body, 1, out, step);
    }

    /* No clause matched. */
    *out = summa_scheme_unspecified();
    return summa_success();
}

/* Both return the operand that decided them, not a boolean, and stop early --
 * which is why neither can be a builtin.
 *
 * The last operand is a tail position in both: whatever it evaluates to is the
 * form's own answer, with nothing left to test. Every operand before it has its
 * truthiness read here, so those recurse. */
static SummaSchemeError summa_scheme_special_and_or(const bool                   stop_on_truthy,
                                                    const SummaSchemeEnvironment env,
                                                    const SummaList              form,
                                                    SummaSchemeValue*            out,
                                                    SummaSchemeStep*             step) {
    if (form->length == 1) {
        /* `(and)` is #t and `(or)` is #f -- the identity of each. */
        *out = summa_make_scheme_boolean(!stop_on_truthy);
        return summa_success();
    }

    for (size_t i = 1; i + 1 < form->length; i++) {
        SummaSchemeValue       next;
        const SummaSchemeError err = summa_scheme_evaluate(env, form->value[i], &next);
        if (err.had) {
            return err;
        }
        if (summa_scheme_truthy(&next) == stop_on_truthy) {
            /* Decided early, and the deciding operand is the answer. */
            *out = next;
            return summa_success();
        }
        summa_scheme_value_free(&next);
    }

    summa_scheme_step_set_tail(step, &form->value[form->length - 1], env);
    return summa_success();
}

static SummaSchemeError summa_scheme_special_and(const SummaSchemeEnvironment env,
                                                 const SummaList              form,
                                                 SummaSchemeValue*            out,
                                                 SummaSchemeStep*             step) {
    return summa_scheme_special_and_or(false, env, form, out, step);
}

static SummaSchemeError summa_scheme_special_or(const SummaSchemeEnvironment env,
                                                const SummaList              form,
                                                SummaSchemeValue*            out,
                                                SummaSchemeStep*             step) {
    return summa_scheme_special_and_or(true, env, form, out, step);
}

static SummaSchemeError summa_scheme_when_unless(const char*                  name,
                                                 const bool                   run_when_truthy,
                                                 const SummaSchemeEnvironment env,
                                                 const SummaList              form,
                                                 SummaSchemeValue*            out,
                                                 SummaSchemeStep*             step) {
    if (form->length < 2) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - expects (%s test body ...)", name, name);
        return summa_make_error(ERROR_MESSAGE);
    }

    SummaSchemeValue       test;
    const SummaSchemeError err = summa_scheme_evaluate(env, form->value[1], &test);
    if (err.had) {
        return err;
    }
    const bool truthy = summa_scheme_truthy(&test);
    summa_scheme_value_free(&test);

    if (truthy != run_when_truthy) {
        *out = summa_scheme_unspecified();
        return summa_success();
    }
    return summa_scheme_evaluate_sequence_tail(env, form, 2, out, step);
}

static SummaSchemeError summa_scheme_special_when(const SummaSchemeEnvironment env,
                                                  const SummaList              form,
                                                  SummaSchemeValue*            out,
                                                  SummaSchemeStep*             step) {
    return summa_scheme_when_unless("when", true, env, form, out, step);
}

static SummaSchemeError summa_scheme_special_unless(const SummaSchemeEnvironment env,
                                                    const SummaList              form,
                                                    SummaSchemeValue*            out,
                                                    SummaSchemeStep*             step) {
    return summa_scheme_when_unless("unless", false, env, form, out, step);
}

typedef struct {
    const char*              name;
    SummaSchemeSpecialFormFn fn;
} SummaSchemeSpecialForm;

/* The six above the line are irreducible. The rest are conveniences a macro
 * expander could rewrite into them, and are cases here only because there is
 * no define-syntax yet. */
static const SummaSchemeSpecialForm SUMMA_SCHEME_SPECIAL_FORMS[] = {
    {"quote", summa_scheme_special_quote},
    {"if", summa_scheme_special_if},
    {"define", summa_scheme_special_define},
    {"lambda", summa_scheme_special_lambda},
    {"set!", summa_scheme_special_set},
    {"begin", summa_scheme_special_begin},

    {"let", summa_scheme_special_let},
    {"let*", summa_scheme_special_let_star},
    {"letrec", summa_scheme_special_letrec},
    {"cond", summa_scheme_special_cond},
    {"and", summa_scheme_special_and},
    {"or", summa_scheme_special_or},
    {"when", summa_scheme_special_when},
    {"unless", summa_scheme_special_unless},
};

#define SUMMA_SCHEME_SPECIAL_FORM_COUNT (sizeof(SUMMA_SCHEME_SPECIAL_FORMS) / sizeof(SUMMA_SCHEME_SPECIAL_FORMS[0]))

static SummaSchemeSymbolName SUMMA_SCHEME_SPECIAL_FORM_SYMBOLS[SUMMA_SCHEME_SPECIAL_FORM_COUNT];

/* Runs once, on the first intern of anything. Every name below is a name the
 * evaluator compares an incoming symbol against, and the comparison is only a
 * pointer compare if both sides came out of the same table -- so they are
 * interned here rather than at each use, where a literal would have to be
 * hashed again on every combination evaluated.
 *
 * The flag is set before the interning rather than after it, because
 * summa_scheme_symbol_intern calls this and the calls below call it back. */
static bool SUMMA_SCHEME_SYMBOLS_READY = false;

static void summa_scheme_symbols_ensure(void) {
    if (SUMMA_SCHEME_SYMBOLS_READY) {
        return;
    }
    SUMMA_SCHEME_SYMBOLS_READY = true;

    for (size_t i = 0; i < SUMMA_SCHEME_SPECIAL_FORM_COUNT; i++) {
        SUMMA_SCHEME_SPECIAL_FORM_SYMBOLS[i] = summa_scheme_symbol_intern(SUMMA_SCHEME_SPECIAL_FORMS[i].name);
    }
    for (size_t i = 0; i < SUMMA_SCHEME_BUILTIN_COUNT; i++) {
        SUMMA_SCHEME_BUILTIN_SYMBOLS[i] = summa_scheme_symbol_intern(SUMMA_SCHEME_BUILTINS[i].name);
    }
    SUMMA_SCHEME_SYMBOL_LAMBDA = summa_scheme_symbol_intern("lambda");
    SUMMA_SCHEME_SYMBOL_ELSE   = summa_scheme_symbol_intern("else");
}

/* Fourteen pointer compares, where this was fourteen strcmps on the head of
 * every combination the evaluator met. */
static SummaSchemeSpecialFormFn summa_scheme_special_form_lookup(SummaSchemeSymbolName name) {
    for (size_t i = 0; i < SUMMA_SCHEME_SPECIAL_FORM_COUNT; i++) {
        if (name == SUMMA_SCHEME_SPECIAL_FORM_SYMBOLS[i]) {
            return SUMMA_SCHEME_SPECIAL_FORMS[i].fn;
        }
    }
    return nullptr;
}

#pragma endregion Special forms

/* Checks the arity and builds the call frame, and stops there. The body is the
 * caller's to run, because a call in tail position runs it in the trampoline
 * rather than under this function. `*out` receives the only reference. */
static SummaSchemeError summa_scheme_procedure_frame(const SummaSchemeEnvironment env,
                                                     const SummaSchemeProcedure   proc,
                                                     const SummaList              args,
                                                     SummaSchemeEnvironment*      out) {
    const size_t expected = proc.bindings ? proc.bindings->length : 0;
    if (expected != args->length) {
        snprintf(ERROR_MESSAGE,
                 ERROR_MESSAGE_LENGTH,
                 "%s - expects %zu argument(s), got %zu",
                 proc.name->value,
                 expected,
                 args->length);
        return summa_make_error(ERROR_MESSAGE);
    }

    /* Lexical scoping: the parent is where the procedure was defined, not
     * where it is called. The fallback only ever covers a builtin.
     *
     * The arity was checked above, so `expected` is exactly what this frame
     * needs -- nothing at all for a call that binds nothing, and no doubling on
     * the way up for a procedure taking more than eight parameters. */
    const SummaSchemeEnvironment frame =
        summa_scheme_environment_make_with_capacity(proc.closure ? proc.closure : env, expected);
    for (size_t i = 0; i < expected; i++) {
        /* The frame takes its own copy of the *value*; the argument list is
         * about to go. The name costs nothing -- it is the same interned record
         * the procedure's parameter list already holds, where it used to be a
         * fresh string allocated per binding per call. */
        SummaSchemeValue argument;
        summa_scheme_value_copy(&argument, &args->value[i]);
        summa_scheme_environment_set(frame, summa_scheme_binding_make(proc.bindings->value[i].value, argument));
    }

    *out = frame;
    return summa_success();
}

/* Calls a procedure and runs it to a value -- the entry point for a host that
 * has the arguments already. The evaluator's own calls go through
 * `summa_scheme_apply`, which stops one step short of this so the trampoline
 * can take the body over. */
SummaSchemeError summa_scheme_procedure_dispatch(SummaSchemeEnvironment env,
                                                 SummaSchemeProcedure   proc,
                                                 const SummaList        args,
                                                 SummaSchemeValue*      out) {
    /* No body means the table owns the behavior. */
    if (!proc.body) {
        return summa_scheme_procedure_dispatch_global(proc, args, out);
    }

    SummaSchemeEnvironment frame = nullptr;
    const SummaSchemeError built = summa_scheme_procedure_frame(env, proc, args, &frame);
    if (built.had) {
        return built;
    }

    const SummaSchemeError err = summa_scheme_evaluate_sequence(frame, proc.body, 0, out);

    /* Evaluation deep-copies, so the result survives the frame -- and a
     * procedure created here holds a reference of its own, so the frame
     * survives with it rather than being freed under it. */
    summa_scheme_environment_release(frame);
    return err;
}

#endif
