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

typedef struct {
    SummaString value;
} SummaSchemeSymbol;

#define summa_make_scheme_symbol(val) \
    ((SummaSchemeValue){.type = SummaSchemeSymbolType, .value.symbol = {.value = summa_string_make(val)}})

SUMMA_ARRAY_GENERATE_TYPE(SummaSchemeSymbolList, symbol_list, SummaSchemeSymbol)

/* A null `body` marks a builtin; dispatch sends it to the procedure table.
 *
 * `closure` is the defining environment, borrowed like
 * SummaSchemeEnvironment_t::parent. A closure that escapes the frame it
 * captured therefore dangles -- see BUILTINS.md. */
typedef struct {
    SummaString            name;
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

typedef struct {
    SummaString      name;
    SummaSchemeValue value;
} SummaSchemeBinding;

#define summa_scheme_binding_make(name_, value_) \
    (SummaSchemeBinding) {                       \
        .name = name_, .value = value_           \
    }

SUMMA_ARRAY_GENERATE_TYPE(SummaSchemeBindingList, binding_list, SummaSchemeBinding)

struct SummaSchemeEnvironment_t {
    SummaSchemeBindingList bindings;
    SummaSchemeEnvironment parent;
};

/* Environments are heap-allocated and returned by value-shaped constructors, so
 * they can outlive the block that built them -- which is what a procedure
 * capturing its defining environment will require.
 *
 * `parent` is borrowed, never owned: freeing a child leaves its parent alone,
 * and a parent must outlive every child pointing at it. That constraint is what
 * gets lifted when environments become reference counted. */
SummaSchemeEnvironment summa_scheme_environment_make(SummaSchemeEnvironment parent);
SummaSchemeEnvironment summa_scheme_environment_make_global(void);

/* Frees the environment, its binding list, and every name and value bound in
 * it -- but not its parent. */
void summa_scheme_environment_free(SummaSchemeEnvironment env);

void summa_scheme_environment_init_global(SummaSchemeEnvironment env);

#define summa_scheme_environment_make_empty() summa_scheme_environment_make(nullptr)

/* Binds `newBinding` in `env`, replacing any binding of the same name. The
 * environment takes ownership of the binding's name and value on both paths --
 * on a rebind it keeps the name it already had and releases the incoming
 * duplicate, so the caller must not free either after the call. */
SummaSchemeError summa_scheme_environment_set(const SummaSchemeEnvironment env, SummaSchemeBinding newBinding);

SummaSchemeError summa_scheme_environment_get(const SummaSchemeEnvironment env,
                                              const SummaSchemeSymbol      symbol,
                                              SummaSchemeBinding*          out);

SummaSchemeError
summa_scheme_environment_get_string(const SummaSchemeEnvironment env, const SummaString str, SummaSchemeBinding* out);

/* Rebinds a name wherever the chain already holds it, which environment_set
 * deliberately does not do. Takes ownership of `value` only on success; an
 * unbound name is an error and leaves it with the caller. */
SummaSchemeError
summa_scheme_environment_assign(const SummaSchemeEnvironment env, const SummaString name, SummaSchemeValue value);

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

/* `print` is R7RS `write` -- strings quoted, so output reads back as source.
 * `display` is the same traversal with strings raw, nested ones included. */
SummaSchemeError summa_scheme_print(const SummaSchemeValue value, FILE* out);
SummaSchemeError summa_scheme_display(const SummaSchemeValue value, FILE* out);

/* Stands in for the missing tail-call optimization: an error beats walking off
 * the C stack. Counted in evaluate frames, several per Scheme-level call. */
#define SUMMA_SCHEME_MAX_DEPTH 2000

#pragma endregion REPL

#endif

#ifdef SUMMA_SCHEME_IMPLEMENTATION

#define ERROR_MESSAGE_LENGTH 1024
char ERROR_MESSAGE[ERROR_MESSAGE_LENGTH];

SummaSchemeError summa_scheme_procedure_dispatch(SummaSchemeEnvironment env,
                                                 SummaSchemeProcedure   proc,
                                                 const SummaList        args,
                                                 SummaSchemeValue*      out);
SummaSchemeError
summa_scheme_evaluate_arguments(const SummaSchemeEnvironment env, const SummaList form, SummaList* out);
void summa_scheme_argument_list_free(SummaList args);

/* Evaluates form[1..], dispatches, releases them. Every call goes through here. */
static SummaSchemeError summa_scheme_apply(const SummaSchemeEnvironment env,
                                           SummaSchemeProcedure         proc,
                                           const SummaList              form,
                                           SummaSchemeValue*            out);

/* Dispatched before the operands are touched -- a special form decides for
 * itself which of them get evaluated. */
typedef SummaSchemeError (*SummaSchemeSpecialFormFn)(const SummaSchemeEnvironment env,
                                                     const SummaList              form,
                                                     SummaSchemeValue*            out);

static SummaSchemeSpecialFormFn summa_scheme_special_form_lookup(const char* name);
static SummaSchemeError         summa_scheme_evaluate_sequence(const SummaSchemeEnvironment env,
                                                               const SummaList              form,
                                                               size_t                       start,
                                                               SummaSchemeValue*            out);

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

#include <ctype.h>

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

SummaSchemeError summa_scheme_read([[maybe_unused]] const SummaSchemeEnvironment env,
                                   [[maybe_unused]] const char*                  input,
                                   [[maybe_unused]] const char**                 rest,
                                   [[maybe_unused]] SummaSchemeValue*            out) {
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
        if (rest) {
            *rest = input + i;
        }

        if (mode != SummaSchemeReadModeString && isblank(c)) {
            if (token->length == 0) {
                continue;
            }
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
    if (mode == SummaSchemeReadModeString) {
        err = summa_make_error("summa_scheme_read - string was not closed");
        goto cleanup;
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
    summa_string_free(token);
    return err;
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

/* All recursion re-enters here, so the depth guard is written once. */
SummaSchemeError
summa_scheme_evaluate(const SummaSchemeEnvironment env, const SummaSchemeValue in, SummaSchemeValue* out) {
    if (!out) {
        return summa_make_error("summa_scheme_evaluate - Out file was null");
    }
    /* Seeded before anything can fail, so every path out has written *out and
     * no error path has to remember to. */
    *out = summa_scheme_unspecified();

    if (SUMMA_SCHEME_DEPTH >= SUMMA_SCHEME_MAX_DEPTH) {
        return summa_make_error("summa_scheme_evaluate - recursion limit exceeded (tail calls are not optimized yet)");
    }
    SUMMA_SCHEME_DEPTH++;
    const SummaSchemeError err = summa_scheme_evaluate_inner(env, in, out);
    SUMMA_SCHEME_DEPTH--;
    return err;
}

static SummaSchemeError summa_scheme_evaluate_inner([[maybe_unused]] const SummaSchemeEnvironment env,
                                                    const SummaSchemeValue                        in,
                                                    SummaSchemeValue*                             out) {
    switch (in.type) {
    case SummaSchemeBooleanType: {
        *out = summa_make_scheme_boolean(in.value.boolean.value);
    } break;
    case SummaSchemeCharacterType: {
        *out = summa_make_scheme_character(in.value.character.value);
    } break;
    case SummaSchemeFloatingType: {
        *out = summa_make_scheme_floating(in.value.floating.value);
    } break;
    case SummaSchemeIntegerType: {
        *out = summa_make_scheme_integer(in.value.integer.value);
    } break;
    case SummaSchemeListType: {
        /* A list in evaluated position is a combination, never data. `(1 2 3)`
         * is an error, and the way to get the list itself is to quote it. */
        SummaList form = in.value.list.value;
        if (!form || form->length == 0) {
            return summa_make_error("Illegal empty combination: ()");
        }

        if (form->value[0].type == SummaSchemeSymbolType) {
            /* Ahead of the binding lookup, deliberately. `if` routed through
             * the application path would evaluate both branches and turn every
             * recursive base case into a loop. */
            const SummaSchemeSpecialFormFn special =
                summa_scheme_special_form_lookup(form->value[0].value.symbol.value->value);
            if (special) {
                return special(env, form, out);
            }

            /* Resolved through the binding rather than by evaluating the
             * symbol, which would deep-copy the procedure on every call. */
            SummaSchemeBinding     head;
            const SummaSchemeError lookup = summa_scheme_environment_get(env, form->value[0].value.symbol, &head);
            if (lookup.had) {
                return lookup;
            }
            if (head.value.type != SummaSchemeProcedureType) {
                return summa_scheme_wrong_type_to_apply(&head.value);
            }
            return summa_scheme_apply(env, head.value.value.procedure, form, out);
        }

        /* Any other operator is an expression: `((lambda (x) x) 1)`, and also
         * `(1 2 3)`, where the 1 evaluates to itself and then fails to apply. */
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
        err = summa_scheme_apply(env, operator_value.value.procedure, form, out);
        summa_scheme_value_free(&operator_value);
        return err;
    } break;
    case SummaSchemeProcedureType: {
        return summa_scheme_value_copy(out, &in);
    } break;
    case SummaSchemeStringType: {
        *out = summa_make_scheme_string(in.value.string.value->value);
    } break;
    case SummaSchemeSymbolType: {
        /* A symbol in evaluated position is a variable reference, so an unbound
         * one is an error rather than a value. Quoting is what yields the
         * symbol itself. */
        SummaSchemeBinding     binding;
        const SummaSchemeError err = summa_scheme_environment_get(env, in.value.symbol, &binding);
        if (err.had) {
            return err;
        }
        /* A copy the caller owns; the binding's value stays the environment's. */
        return summa_scheme_value_copy(out, &binding.value);
    } break;
    case SummaSchemeVectorType: {
        /* Self-evaluating, unlike a list. */
        return summa_scheme_value_copy(out, &in);
    } break;
    default: {
        return summa_make_error("summa_scheme_evaluate - Invalid in type");
    }
    }

    return summa_success();
}

static SummaSchemeError summa_scheme_print_styled(const SummaSchemeValue value, FILE* out, bool quote_strings);

SummaSchemeError summa_scheme_print(const SummaSchemeValue value, FILE* out) {
    return summa_scheme_print_styled(value, out, true);
}

SummaSchemeError summa_scheme_display(const SummaSchemeValue value, FILE* out) {
    return summa_scheme_print_styled(value, out, false);
}

static SummaSchemeError summa_scheme_print_styled(const SummaSchemeValue value, FILE* out, bool quote_strings) {
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
        SummaSchemeCharacter val = value.value.character;
        fprintf(out, "%c", val.value);
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
            summa_scheme_print_styled(next_value, out, quote_strings);
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
        fprintf(out, quote_strings ? "\"%s\"" : "%s", str->value);
    } break;
    case SummaSchemeSymbolType: {
        SummaSchemeSymbol val = value.value.symbol;
        SummaString       str = val.value;
        fprintf(out, "%s", str->value);
    } break;
    case SummaSchemeVectorType: {
        SummaSchemeVector val = value.value.vector;
        fprintf(out, "#(");
        for (size_t i = 0; i < val.value->length; i++) {
            if (i != 0) {
                fprintf(out, " ");
            }
            SummaSchemeValue next_value = val.value->value[i];
            summa_scheme_print_styled(next_value, out, quote_strings);
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
 * handle inside them. These walk instead. */
static SummaList summa_scheme_list_copy_deep(const SummaList src) {
    if (!src) {
        return nullptr;
    }
    SummaList dest = summa_list_make_empty();
    for (size_t i = 0; i < src->length; i++) {
        SummaSchemeValue element;
        summa_scheme_value_copy(&element, &src->value[i]);
        summa_list_push(dest, &element);
    }
    return dest;
}

static SummaSchemeSymbolList summa_scheme_symbol_list_copy_deep(const SummaSchemeSymbolList src) {
    if (!src) {
        return nullptr;
    }
    SummaSchemeSymbolList dest = summa_symbol_list_make_empty();
    for (size_t i = 0; i < src->length; i++) {
        SummaSchemeSymbol symbol = {.value = summa_string_make(src->value[i].value->value)};
        summa_symbol_list_push(dest, &symbol);
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

static void summa_scheme_symbol_list_free_deep(SummaSchemeSymbolList symbols) {
    if (!symbols) {
        return;
    }
    for (size_t i = 0; i < symbols->length; i++) {
        summa_string_free(symbols->value[i].value);
    }
    summa_symbol_list_free(symbols);
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
        dest->value.procedure.name     = summa_string_make(src->value.procedure.name->value);
        dest->value.procedure.bindings = nullptr;
        dest->value.procedure.body     = nullptr;
        /* Borrowed: not the procedure's to duplicate or release. */
        dest->value.procedure.closure = src->value.procedure.closure;
        if (src->value.procedure.bindings) {
            dest->value.procedure.bindings = summa_scheme_symbol_list_copy_deep(src->value.procedure.bindings);
        }
        if (src->value.procedure.body) {
            dest->value.procedure.body = summa_scheme_list_copy_deep(src->value.procedure.body);
        }
    } break;
    case SummaSchemeStringType: {
        dest->value.string.value = summa_string_make(src->value.string.value->value);
    } break;
    case SummaSchemeSymbolType: {
        dest->value.symbol.value = summa_string_make(src->value.symbol.value->value);
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
    case SummaSchemeListType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.list.value, summa_scheme_list_free_deep);
    } break;
    case SummaSchemeProcedureType: {
        /* A procedure can carry no bindings or no body, so each handle is
         * checked before release. `closure` is borrowed and deliberately
         * absent -- freeing a procedure must not disturb the environment it
         * was defined in. */
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.name, summa_string_free);
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.bindings, summa_scheme_symbol_list_free_deep);
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.body, summa_scheme_list_free_deep);
    } break;
    case SummaSchemeStringType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.string.value, summa_string_free);
    } break;
    case SummaSchemeSymbolType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.symbol.value, summa_string_free);
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
        return summa_string_cmp(left->value.procedure.name, right->value.procedure.name) == 0;
    }
    case SummaSchemeStringType: {
        return summa_string_cmp(left->value.string.value, right->value.string.value) == 0;
    }
    case SummaSchemeSymbolType: {
        // TODO: Make this better by looking up in the environment's symbol map for equality
        return summa_string_cmp(left->value.symbol.value, right->value.symbol.value) == 0;
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

SummaSchemeEnvironment summa_scheme_environment_make(SummaSchemeEnvironment parent) {
    SummaSchemeEnvironment env = malloc(sizeof(struct SummaSchemeEnvironment_t));
    assert(env);
    env->bindings = summa_binding_list_make_empty();
    env->parent   = parent;
    return env;
}

SummaSchemeEnvironment summa_scheme_environment_make_global(void) {
    SummaSchemeEnvironment env = summa_scheme_environment_make(nullptr);
    summa_scheme_environment_init_global(env);
    return env;
}

void summa_scheme_environment_free(SummaSchemeEnvironment env) {
    if (!env) {
        return;
    }
    for (size_t i = 0; i < env->bindings->length; i++) {
        SummaSchemeBinding* binding = &env->bindings->value[i];
        summa_scheme_value_free(&binding->value);
        summa_string_free(binding->name);
    }
    summa_binding_list_free(env->bindings);
    /* parent is borrowed, not owned. */
    free(env);
}

/* The table lives with the builtins; this only needs the names. */
static size_t      summa_scheme_builtin_count(void);
static const char* summa_scheme_builtin_name(size_t index);

void summa_scheme_environment_init_global(SummaSchemeEnvironment env) {
    SummaSchemeBindingList bindings = env->bindings;

    for (size_t i = 0; i < summa_scheme_builtin_count(); i++) {
        const char* name = summa_scheme_builtin_name(i);
        /* The binding's name and the procedure's name are separate strings on
         * purpose: one handle shared across both would be freed twice. */
        summa_binding_list_push(
            bindings,
            &summa_scheme_binding_make(
                summa_string_make(name),
                summa_make_scheme_procedure(summa_string_make(name), summa_symbol_list_make_empty(), nullptr)));
    }
}

SummaSchemeError summa_scheme_environment_set(const SummaSchemeEnvironment env, SummaSchemeBinding newBinding) {
    for (size_t i = 0; i < env->bindings->length; i++) {
        /* By reference: a copy of the element would take the rebinding with it
         * when it went out of scope, leaving the environment untouched. */
        SummaSchemeBinding* binding = &env->bindings->value[i];
        if (summa_string_cmp(binding->name, newBinding.name) == 0) {
            /* The old value is being replaced, so it goes before the overwrite
             * rather than after it. The incoming value is moved in rather than
             * copied, and the incoming name is released -- the environment
             * already holds an equal one, and set() owns everything it is
             * handed on both paths. */
            summa_scheme_value_free(&binding->value);
            binding->value = newBinding.value;
            summa_string_free(newBinding.name);
            return summa_success();
        }
    }
    /* Pushed by value: the environment takes ownership of the binding's name
     * and value from here on. */
    summa_binding_list_push(env->bindings, &newBinding);
    return summa_success();
}

SummaSchemeError summa_scheme_environment_get(const SummaSchemeEnvironment env,
                                              const SummaSchemeSymbol      symbol,
                                              SummaSchemeBinding*          out) {
    return summa_scheme_environment_get_string(env, symbol.value, out);
}

SummaSchemeError
summa_scheme_environment_get_string(const SummaSchemeEnvironment env, const SummaString str, SummaSchemeBinding* out) {
    for (size_t i = 0; i < env->bindings->length; i++) {
        SummaSchemeBinding binding = env->bindings->value[i];
        if (summa_string_cmp(binding.name, str) == 0) {
            *out = binding;
            return summa_success();
        }
    }
    if (env->parent) {
        return summa_scheme_environment_get_string(env->parent, str, out);
    }
    snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "Unbound variable: %s", str->value);
    return summa_make_error(ERROR_MESSAGE);
}

SummaSchemeError
summa_scheme_environment_assign(const SummaSchemeEnvironment env, const SummaString name, SummaSchemeValue value) {
    for (size_t i = 0; i < env->bindings->length; i++) {
        /* By reference: a copy would carry the rebinding out of scope. */
        SummaSchemeBinding* binding = &env->bindings->value[i];
        if (summa_string_cmp(binding->name, name) == 0) {
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
    SummaList args = summa_list_make_empty();
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

static SummaSchemeError summa_scheme_apply(const SummaSchemeEnvironment env,
                                           SummaSchemeProcedure         proc,
                                           const SummaList              form,
                                           SummaSchemeValue*            out) {
    SummaList        args = nullptr;
    SummaSchemeError err  = summa_scheme_evaluate_arguments(env, form, &args);
    if (!err.had) {
        err = summa_scheme_procedure_dispatch(env, proc, args, out);
    }
    /* Arguments die with the call; dispatch copies out anything it returns. */
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

/* Unused until a fixed-arity builtin lands; `+` is variadic. */
[[maybe_unused]] static SummaSchemeError
summa_scheme_require_arity(const char* name, const SummaList args, size_t expected) {
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

/* One function plus one row: the global environment binds a procedure per row
 * at startup, and dispatch finds it by name. */
static const SummaSchemeBuiltin SUMMA_SCHEME_BUILTINS[] = {
    {"+", summa_scheme_builtin_add},
};

static size_t summa_scheme_builtin_count(void) {
    return sizeof(SUMMA_SCHEME_BUILTINS) / sizeof(SUMMA_SCHEME_BUILTINS[0]);
}

static const char* summa_scheme_builtin_name(size_t index) {
    return SUMMA_SCHEME_BUILTINS[index].name;
}

SummaSchemeError
summa_scheme_procedure_dispatch_global(SummaSchemeProcedure proc, const SummaList args, SummaSchemeValue* out) {
    for (size_t i = 0; i < summa_scheme_builtin_count(); i++) {
        if (strcmp(proc.name->value, SUMMA_SCHEME_BUILTINS[i].name) == 0) {
            return SUMMA_SCHEME_BUILTINS[i].fn(args, out);
        }
    }
    snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "Unknown builtin procedure: %s", proc.name->value);
    return summa_make_error(ERROR_MESSAGE);
}

#pragma endregion Builtin procedures

#pragma region Special forms

/* Evaluates form[start..] and yields the last value -- the body rule shared by
 * lambda, begin, let, cond, when and unless. An empty range is not an error. */
static SummaSchemeError summa_scheme_evaluate_sequence(const SummaSchemeEnvironment env,
                                                       const SummaList              form,
                                                       size_t                       start,
                                                       SummaSchemeValue*            out) {
    SummaSchemeValue result = summa_scheme_unspecified();
    for (size_t i = start; i < form->length; i++) {
        SummaSchemeValue       next;
        const SummaSchemeError err = summa_scheme_evaluate(env, form->value[i], &next);
        if (err.had) {
            summa_scheme_value_free(&result);
            return err;
        }
        summa_scheme_value_free(&result);
        result = next;
    }
    *out = result;
    return summa_success();
}

static SummaSchemeError summa_scheme_special_quote([[maybe_unused]] const SummaSchemeEnvironment env,
                                                   const SummaList                               form,
                                                   SummaSchemeValue*                             out) {
    if (form->length != 2) {
        return summa_make_error("quote - expects exactly one operand");
    }
    /* The point of the form: copied, never evaluated. */
    return summa_scheme_value_copy(out, &form->value[1]);
}

static SummaSchemeError
summa_scheme_special_if(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    if (form->length < 3 || form->length > 4) {
        return summa_make_error("if - expects (if test consequent [alternate])");
    }

    SummaSchemeValue       test;
    const SummaSchemeError err = summa_scheme_evaluate(env, form->value[1], &test);
    if (err.had) {
        return err;
    }
    const bool truthy = summa_scheme_truthy(&test);
    summa_scheme_value_free(&test);

    /* Exactly one branch. Recursion terminating rests on this. */
    if (truthy) {
        return summa_scheme_evaluate(env, form->value[2], out);
    }
    if (form->length == 4) {
        return summa_scheme_evaluate(env, form->value[3], out);
    }
    *out = summa_scheme_unspecified();
    return summa_success();
}

/* Parameter names and body copied out; defining environment captured by
 * handle. `params` is only read -- its elements stay the caller's. */
static SummaSchemeError summa_scheme_make_closure(const SummaSchemeEnvironment env,
                                                  const char*                  name,
                                                  const SummaList              params,
                                                  const SummaList              form,
                                                  size_t                       body_start,
                                                  SummaSchemeValue*            out) {
    if (form->length <= body_start) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - expects at least one body expression", name);
        return summa_make_error(ERROR_MESSAGE);
    }

    SummaSchemeSymbolList bindings = summa_symbol_list_make_empty();
    for (size_t i = 0; params && i < params->length; i++) {
        if (params->value[i].type != SummaSchemeSymbolType) {
            summa_scheme_symbol_list_free_deep(bindings);
            snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - parameter names must be symbols", name);
            return summa_make_error(ERROR_MESSAGE);
        }
        SummaSchemeSymbol symbol = {.value = summa_string_make(params->value[i].value.symbol.value->value)};
        summa_symbol_list_push(bindings, &symbol);
    }

    SummaList body = summa_list_make_empty();
    for (size_t i = body_start; i < form->length; i++) {
        SummaSchemeValue expression;
        summa_scheme_value_copy(&expression, &form->value[i]);
        summa_list_push(body, &expression);
    }

    *out = summa_make_scheme_closure(summa_string_make(name), bindings, body, env);
    return summa_success();
}

static SummaSchemeError
summa_scheme_special_lambda(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    if (form->length < 3) {
        return summa_make_error("lambda - expects (lambda (parameters ...) body ...)");
    }
    if (form->value[1].type != SummaSchemeListType) {
        return summa_make_error("lambda - parameter list must be a list");
    }
    return summa_scheme_make_closure(env, "lambda", form->value[1].value.list.value, form, 2, out);
}

static SummaSchemeError
summa_scheme_special_define(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
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
        const char* name = signature->value[0].value.symbol.value->value;

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
        summa_scheme_environment_set(env, summa_scheme_binding_make(summa_string_make(name), procedure));
        *out = summa_make_scheme_symbol(name);
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
    /* Separate strings: the binding name, the value, and the symbol returned
     * are all freed by different owners. */
    const char* name = target.value.symbol.value->value;
    summa_scheme_environment_set(env, summa_scheme_binding_make(summa_string_make(name), value));
    *out = summa_make_scheme_symbol(name);
    return summa_success();
}

static SummaSchemeError
summa_scheme_special_set(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
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

static SummaSchemeError
summa_scheme_special_begin(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    return summa_scheme_evaluate_sequence(env, form, 1, out);
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
                                                const char**             out_name,
                                                const SummaSchemeValue** out_expression) {
    if (clause->type != SummaSchemeListType || !clause->value.list.value || clause->value.list.value->length != 2 ||
        clause->value.list.value->value[0].type != SummaSchemeSymbolType) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - each binding must be (name value)", name);
        return summa_make_error(ERROR_MESSAGE);
    }
    *out_name       = clause->value.list.value->value[0].value.symbol.value->value;
    *out_expression = &clause->value.list.value->value[1];
    return summa_success();
}

static SummaSchemeError summa_scheme_let(const char*                  name,
                                         SummaSchemeLetKind           kind,
                                         const SummaSchemeEnvironment env,
                                         const SummaList              form,
                                         SummaSchemeValue*            out) {
    if (form->length < 3) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - expects (%s ((name value) ...) body ...)", name, name);
        return summa_make_error(ERROR_MESSAGE);
    }
    if (form->value[1].type != SummaSchemeListType) {
        snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "%s - bindings must be a list", name);
        return summa_make_error(ERROR_MESSAGE);
    }

    const SummaList              clauses = form->value[1].value.list.value;
    const SummaSchemeEnvironment frame   = summa_scheme_environment_make(env);
    SummaSchemeError             err     = summa_success();

    if (kind == SUMMA_SCHEME_LET_RECURSIVE) {
        /* Names first, values second -- the gap is the point of letrec. */
        for (size_t i = 0; clauses && i < clauses->length; i++) {
            const char*             binding_name = nullptr;
            const SummaSchemeValue* expression   = nullptr;
            err = summa_scheme_let_clause(name, &clauses->value[i], &binding_name, &expression);
            if (err.had) {
                break;
            }
            summa_scheme_environment_set(
                frame, summa_scheme_binding_make(summa_string_make(binding_name), summa_scheme_unspecified()));
        }
    }

    for (size_t i = 0; !err.had && clauses && i < clauses->length; i++) {
        const char*             binding_name = nullptr;
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
        summa_scheme_environment_set(frame, summa_scheme_binding_make(summa_string_make(binding_name), value));
    }

    if (!err.had) {
        err = summa_scheme_evaluate_sequence(frame, form, 2, out);
    }
    summa_scheme_environment_free(frame);
    return err;
}

static SummaSchemeError
summa_scheme_special_let(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    return summa_scheme_let("let", SUMMA_SCHEME_LET_PARALLEL, env, form, out);
}

static SummaSchemeError
summa_scheme_special_let_star(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    return summa_scheme_let("let*", SUMMA_SCHEME_LET_SEQUENTIAL, env, form, out);
}

static SummaSchemeError
summa_scheme_special_letrec(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    return summa_scheme_let("letrec", SUMMA_SCHEME_LET_RECURSIVE, env, form, out);
}

static SummaSchemeError
summa_scheme_special_cond(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    for (size_t i = 1; i < form->length; i++) {
        const SummaSchemeValue clause = form->value[i];
        if (clause.type != SummaSchemeListType || !clause.value.list.value || clause.value.list.value->length == 0) {
            return summa_make_error("cond - each clause must be (test expression ...)");
        }
        const SummaList body = clause.value.list.value;

        const bool is_else = body->value[0].type == SummaSchemeSymbolType &&
                             strcmp(body->value[0].value.symbol.value->value, "else") == 0;
        if (is_else) {
            return summa_scheme_evaluate_sequence(env, body, 1, out);
        }

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
        return summa_scheme_evaluate_sequence(env, body, 1, out);
    }

    /* No clause matched. */
    *out = summa_scheme_unspecified();
    return summa_success();
}

/* Both return the operand that decided them, not a boolean, and stop early --
 * which is why neither can be a builtin. */
static SummaSchemeError
summa_scheme_special_and(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    SummaSchemeValue result = summa_make_scheme_boolean(true);
    for (size_t i = 1; i < form->length; i++) {
        SummaSchemeValue       next;
        const SummaSchemeError err = summa_scheme_evaluate(env, form->value[i], &next);
        if (err.had) {
            summa_scheme_value_free(&result);
            return err;
        }
        summa_scheme_value_free(&result);
        result = next;
        if (!summa_scheme_truthy(&result)) {
            break;
        }
    }
    *out = result;
    return summa_success();
}

static SummaSchemeError
summa_scheme_special_or(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    SummaSchemeValue result = summa_make_scheme_boolean(false);
    for (size_t i = 1; i < form->length; i++) {
        SummaSchemeValue       next;
        const SummaSchemeError err = summa_scheme_evaluate(env, form->value[i], &next);
        if (err.had) {
            summa_scheme_value_free(&result);
            return err;
        }
        summa_scheme_value_free(&result);
        result = next;
        if (summa_scheme_truthy(&result)) {
            break;
        }
    }
    *out = result;
    return summa_success();
}

static SummaSchemeError summa_scheme_when_unless(const char*                  name,
                                                 bool                         run_when_truthy,
                                                 const SummaSchemeEnvironment env,
                                                 const SummaList              form,
                                                 SummaSchemeValue*            out) {
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
    return summa_scheme_evaluate_sequence(env, form, 2, out);
}

static SummaSchemeError
summa_scheme_special_when(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    return summa_scheme_when_unless("when", true, env, form, out);
}

static SummaSchemeError
summa_scheme_special_unless(const SummaSchemeEnvironment env, const SummaList form, SummaSchemeValue* out) {
    return summa_scheme_when_unless("unless", false, env, form, out);
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

static SummaSchemeSpecialFormFn summa_scheme_special_form_lookup(const char* name) {
    const size_t count = sizeof(SUMMA_SCHEME_SPECIAL_FORMS) / sizeof(SUMMA_SCHEME_SPECIAL_FORMS[0]);
    for (size_t i = 0; i < count; i++) {
        if (strcmp(name, SUMMA_SCHEME_SPECIAL_FORMS[i].name) == 0) {
            return SUMMA_SCHEME_SPECIAL_FORMS[i].fn;
        }
    }
    return nullptr;
}

#pragma endregion Special forms

SummaSchemeError summa_scheme_procedure_dispatch(SummaSchemeEnvironment env,
                                                 SummaSchemeProcedure   proc,
                                                 const SummaList        args,
                                                 SummaSchemeValue*      out) {
    /* No body means the table owns the behavior. */
    if (!proc.body) {
        return summa_scheme_procedure_dispatch_global(proc, args, out);
    }

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
     * where it is called. The fallback only ever covers a builtin. */
    const SummaSchemeEnvironment frame = summa_scheme_environment_make(proc.closure ? proc.closure : env);
    for (size_t i = 0; i < expected; i++) {
        /* The frame takes its own copy; the argument list is about to go. */
        SummaSchemeValue argument;
        summa_scheme_value_copy(&argument, &args->value[i]);
        summa_scheme_environment_set(
            frame, summa_scheme_binding_make(summa_string_make(proc.bindings->value[i].value->value), argument));
    }

    const SummaSchemeError err = summa_scheme_evaluate_sequence(frame, proc.body, 0, out);

    /* Evaluation deep-copies, so the result survives the frame -- except for a
     * procedure created here, whose closure handle this leaves dangling. See
     * BUILTINS.md. */
    summa_scheme_environment_free(frame);
    return err;
}

#endif
