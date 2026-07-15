#ifndef SUMMA_SCHEME_H
#define SUMMA_SCHEME_H

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

#pragma region Expression

typedef struct {
    // TODO: placeholder
    void* value;
} SummaSchemeExpression;

#pragma endregion Expression

#pragma region Values

typedef struct SummaSchemeValue SummaSchemeValue;

typedef enum {
    SchemeBoolean,
    SchemeCharacter,
    SchemeFloating,
    SchemeInteger,
    SchemeList,
    SchemeProcedure,
    SchemeString,
    SchemeSymbol,
    SchemeVector,
} SummaSchemeValueType;

typedef struct {
    bool value;
} SummaSchemeBoolean;

#define summa_make_scheme_boolean(val) ((SummaSchemeValue){.type = SchemeBoolean, .value.boolean = {.value = (val)}})

#define SUMMA_SCHEME_TRUE "#t"
#define SUMMA_SCHEME_FALSE "#f"

typedef struct {
    char value;
} SummaSchemeCharacter;

#define summa_make_scheme_character(val) \
    ((SummaSchemeValue){.type = SchemeCharacter, .value.character = {.value = (val)}})

typedef struct {
    double value;
} SummaSchemeFloating;

#define summa_make_scheme_floating(val) \
    ((SummaSchemeValue){.type = SchemeFloating, .value.floating = {.value = (val)}})

typedef struct {
    int64_t value;
} SummaSchemeInteger;

#define summa_make_scheme_integer(val) ((SummaSchemeValue){.type = SchemeInteger, .value.integer = {.value = (val)}})

SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaList, list, SummaSchemeValue)

typedef struct {
    SummaList value;
} SummaSchemeList;

#define summa_make_scheme_list(val) ((SummaSchemeValue){.type = SchemeList, .value.list = {.value = (val)}})

typedef struct {
    // TODO: placeholder
    void* value;
} SummaSchemeProcedure;

#define summa_make_scheme_procedure(val) \
    ((SummaSchemeValue){.type = SchemeProcedure, .value.procedure = {.value = (val)}})

typedef struct {
    SummaString value;
} SummaSchemeString;

#define summa_make_scheme_string(val) \
    ((SummaSchemeValue){.type = SchemeString, .value.string = {.value = summa_string_make(val)}})

typedef struct {
    // TODO: placeholder
    void* value;
} SummaSchemeSymbol;

#define summa_make_scheme_symbol(val) ((SummaSchemeValue){.type = SchemeSymbol, .value.symbol = {.value = (val)}})

typedef struct {
    // TODO: placeholder
    void* value;
} SummaSchemeVector;

#define summa_make_scheme_vector(val) ((SummaSchemeValue){.type = SchemeVector, .value.vector = {.value = (val)}})

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

#pragma endregion Values

#pragma region REPL

SummaSchemeError summa_scheme_read(const char* inputText, SummaSchemeExpression* out);
SummaSchemeError summa_scheme_evaluate(const SummaSchemeExpression expression, SummaSchemeValue* out);
SummaSchemeError summa_scheme_print(const SummaSchemeValue value, FILE* out);

#pragma endregion REPL

#endif

#ifdef SUMMA_SCHEME_IMPLEMENTATION

SUMMA_ARRAY_GENERATE_TYPE_IMPL(SummaList, list, SummaSchemeValue)

SummaSchemeError summa_scheme_read([[maybe_unused]] const char*            inputText,
                                   [[maybe_unused]] SummaSchemeExpression* out) {
    return summa_make_error("summa_scheme_read - NOT IMPLEMENTED");
}
SummaSchemeError summa_scheme_evaluate([[maybe_unused]] const SummaSchemeExpression expression,
                                       [[maybe_unused]] SummaSchemeValue*           out) {
    return summa_make_error("summa_scheme_evaluate - NOT IMPLEMENTED");
}

SummaSchemeError summa_scheme_print([[maybe_unused]] const SummaSchemeValue value, [[maybe_unused]] FILE* out) {
    if (!out) {
        return summa_make_error("summa_scheme_print - Out file was null");
    }

    switch (value.type) {
    case SchemeBoolean: {
        SummaSchemeBoolean val = value.value.boolean;
        if (val.value) {
            fprintf(out, SUMMA_SCHEME_TRUE);
        } else {
            fprintf(out, SUMMA_SCHEME_FALSE);
        }
    } break;
    case SchemeCharacter: {
        SummaSchemeCharacter val = value.value.character;
        fprintf(out, "%c", val.value);
    } break;
    case SchemeFloating: {
        SummaSchemeFloating val = value.value.floating;
        fprintf(out, "%f", val.value);
    } break;
    case SchemeInteger: {
        SummaSchemeInteger val = value.value.integer;
        fprintf(out, "%" PRId64, val.value);
    } break;
    case SchemeList: {
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
    case SchemeProcedure: {
        return summa_make_error("summa_scheme_print - NOT IMPLEMENTED - SchemeProcedure");
    } break;
    case SchemeString: {
        SummaSchemeString val = value.value.string;
        SummaString       str = val.value;
        fprintf(out, "\"%s\"", str->value);
    } break;
    case SchemeSymbol: {
        return summa_make_error("summa_scheme_print - NOT IMPLEMENTED - SchemeSymbol");
    } break;
    case SchemeVector: {
        return summa_make_error("summa_scheme_print - NOT IMPLEMENTED - SchemeVector");
    } break;
    default: {
        return summa_make_error("summa_scheme_print - Invalid value type");
    }
    }

    return summa_success();
}

#endif
