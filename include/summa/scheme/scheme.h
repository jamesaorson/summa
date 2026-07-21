#ifndef SUMMA_SCHEME_H
#define SUMMA_SCHEME_H

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
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

SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaList, list, SummaSchemeValue)

SummaSchemeError summa_scheme_value_copy(SummaSchemeValue* dest, const SummaSchemeValue* src);
bool             summa_scheme_value_equals(const SummaSchemeValue* left, const SummaSchemeValue* right);

/* Releases exactly what summa_scheme_value_copy allocates: the container
 * handles and strings the value itself owns.
 *
 * Deliberately shallow, to mirror the copy it undoes. summa_list_copy and
 * summa_symbol_list_copy move elements as raw bytes, so a copied list shares
 * every handle sitting inside its elements with the original -- freeing those
 * here would double-free them. Whoever built the elements frees the elements. */
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

typedef struct {
    SummaString           name;
    SummaSchemeSymbolList bindings;
    SummaList             body;
} SummaSchemeProcedure;

#define summa_make_scheme_procedure(name_, bindings_, body_)         \
    ((SummaSchemeValue){.type            = SummaSchemeProcedureType, \
                        .value.procedure = {.name = (name_), .bindings = (bindings_), .body = (body_)}})

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

typedef struct SummaSchemeEnvironment_t* SummaSchemeEnvironment;
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

#pragma endregion Values

#pragma region REPL

SummaSchemeError summa_scheme_read(const SummaSchemeEnvironment env, const char* inputText, SummaSchemeValue* out);
SummaSchemeError
summa_scheme_evaluate(const SummaSchemeEnvironment env, const SummaSchemeValue in, SummaSchemeValue* out);
SummaSchemeError summa_scheme_print(const SummaSchemeValue value, FILE* out);

#pragma endregion REPL

#endif

#ifdef SUMMA_SCHEME_IMPLEMENTATION

#define ERROR_MESSAGE_LENGTH 1024
char ERROR_MESSAGE[ERROR_MESSAGE_LENGTH];

SummaSchemeError summa_scheme_read([[maybe_unused]] const SummaSchemeEnvironment env,
                                   [[maybe_unused]] const char*                  inputText,
                                   [[maybe_unused]] SummaSchemeValue*            out) {
    return summa_make_error("summa_scheme_read - NOT IMPLEMENTED");
}

SummaSchemeError summa_scheme_evaluate([[maybe_unused]] const SummaSchemeEnvironment env,
                                       const SummaSchemeValue                        in,
                                       SummaSchemeValue*                             out) {
    // TODO: Check if the in value has been defined before. If so, return the SummaSchemeValue* of the global value.
    if (!out) {
        return summa_make_error("summa_scheme_evaluate - Out file was null");
    }

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
        return summa_scheme_value_copy(out, &in);
    } break;
    case SummaSchemeProcedureType: {
        return summa_make_error("summa_scheme_evaluate - procedure - NOT IMPLEMENTED");
    } break;
    case SummaSchemeStringType: {
        *out = summa_make_scheme_string(in.value.string.value->value);
    } break;
    case SummaSchemeSymbolType: {
        SummaSchemeBinding binding;
        SummaSchemeError   err = summa_scheme_environment_get(env, in.value.symbol, &binding);
        if (err.had) {
            /* Unbound, so the symbol evaluates to itself and is interned into
             * the environment on the way out.
             *
             * The name, the symbol bound to it, and the one returned are three
             * separate strings on purpose -- the same reason the global `+`
             * binding builds two. The environment frees a binding's value and
             * its name independently, and the caller frees what it gets back,
             * so any handle shared between those would be released twice. */
            const char* name = in.value.symbol.value->value;
            summa_scheme_environment_set(
                env, summa_scheme_binding_make(summa_string_make(name), summa_make_scheme_symbol(name)));
        }
        /* Either branch hands back a copy the caller owns: the binding's value
         * stays the environment's. */
        return summa_scheme_value_copy(out, err.had ? &in : &binding.value);
    } break;
    case SummaSchemeVectorType: {
        return summa_scheme_value_copy(out, &in);
    } break;
    default: {
        return summa_make_error("summa_scheme_evaluate - Invalid in type");
    }
    }

    return summa_success();
}

SummaSchemeError summa_scheme_print(const SummaSchemeValue value, FILE* out) {
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
            summa_scheme_print(next_value, out);
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
        fprintf(out, "\"%s\"", str->value);
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
            summa_scheme_print(next_value, out);
        }
        fprintf(out, ")");
    } break;
    default: {
        return summa_make_error("summa_scheme_print - Invalid value type");
    }
    }

    return summa_success();
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
        dest->value.list.value = summa_list_make_empty();
        summa_list_copy(dest->value.list.value, src->value.list.value);
    } break;
    case SummaSchemeProcedureType: {
        /* dest's handles are whatever the caller happened to have there, so
         * each one is built fresh before being copied into -- the same shape
         * the list and vector branches use. A procedure may legitimately carry
         * no bindings or no body, and a null handle stays null. */
        dest->value.procedure.name     = summa_string_make(src->value.procedure.name->value);
        dest->value.procedure.bindings = nullptr;
        dest->value.procedure.body     = nullptr;
        if (src->value.procedure.bindings) {
            dest->value.procedure.bindings = summa_symbol_list_make_empty();
            summa_symbol_list_copy(dest->value.procedure.bindings, src->value.procedure.bindings);
        }
        if (src->value.procedure.body) {
            dest->value.procedure.body = summa_list_make_empty();
            summa_list_copy(dest->value.procedure.body, src->value.procedure.body);
        }
    } break;
    case SummaSchemeStringType: {
        dest->value.string.value = summa_string_make(src->value.string.value->value);
    } break;
    case SummaSchemeSymbolType: {
        dest->value.symbol.value = summa_string_make(src->value.symbol.value->value);
    } break;
    case SummaSchemeVectorType: {
        dest->value.vector.value = summa_list_make_empty();
        summa_list_copy(dest->value.vector.value, src->value.vector.value);
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
        SUMMA_SCHEME_FREE_HANDLE(value->value.list.value, summa_list_free);
    } break;
    case SummaSchemeProcedureType: {
        /* A procedure can carry no bindings or no body, so each handle is
         * checked before release. */
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.name, summa_string_free);
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.bindings, summa_symbol_list_free);
        SUMMA_SCHEME_FREE_HANDLE(value->value.procedure.body, summa_list_free);
    } break;
    case SummaSchemeStringType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.string.value, summa_string_free);
    } break;
    case SummaSchemeSymbolType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.symbol.value, summa_string_free);
    } break;
    case SummaSchemeVectorType: {
        SUMMA_SCHEME_FREE_HANDLE(value->value.vector.value, summa_list_free);
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

void summa_scheme_environment_init_global(SummaSchemeEnvironment env) {
    SummaSchemeBindingList bindings = env->bindings;

    /* The binding's name and the procedure's name are two separate strings on
     * purpose. Sharing one handle across both would make the binding and the
     * value it holds co-owners of it, and freeing the environment would then
     * release it twice. */
    SummaString bindingName   = summa_string_make("+");
    SummaString procedureName = summa_string_make("+");
    summa_binding_list_push(
        bindings,
        &summa_scheme_binding_make(
            bindingName,
            summa_make_scheme_procedure(procedureName, summa_symbol_list_make_empty(), summa_list_make_empty())));
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
    for (size_t i = 0; i < env->bindings->length; i++) {
        SummaSchemeBinding binding = env->bindings->value[i];
        if (summa_string_cmp(binding.name, symbol.value) == 0) {
            *out = binding;
            return summa_success();
        }
    }
    if (env->parent) {
        return summa_scheme_environment_get(env->parent, symbol, out);
    }
    snprintf(ERROR_MESSAGE, ERROR_MESSAGE_LENGTH, "Unbound variable: %s", symbol.value->value);
    return summa_make_error(ERROR_MESSAGE);
}

#endif
