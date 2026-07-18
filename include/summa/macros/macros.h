#ifndef SUMMA_MACROS_H
#define SUMMA_MACROS_H

// Source - https://stackoverflow.com/a/1597129
// Posted by Adam Rosenfield, modified by community. See post 'Timeline' for change history
// Retrieved 2026-07-15, License - CC BY-SA 3.0

#define _SUMMA_TOKEN_CONCAT2_IMPL(x, y) x##y
#define SUMMA_TOKEN_CONCAT2(x, y) _SUMMA_TOKEN_CONCAT2_IMPL(x, y)

#define _SUMMA_TOKEN_CONCAT3_IMPL(x, y, z) x##y##z
#define SUMMA_TOKEN_CONCAT3(x, y, z) _SUMMA_TOKEN_CONCAT3_IMPL(x, y, z)

#endif