// crt_compat.cpp — Xbox CRT compatibility shims
//
// Provides MSVC intrinsics that the Xbox CRT doesn't implement.
// Add this file to the project once; it covers the entire build.
//
// __ftol2_sse: MSVC emits this for float/double → int truncation when
// compiling with SSE codegen. The Xbox CPU has no SSE2; the symbol is
// absent from the Xbox CRT. We implement it with a plain C cast which
// the compiler will lower to a FPU FISTP instruction instead.

#include <xtl.h>

// __ftol2_sse — MSVC calls this when converting float/double to long.
// The value is already in ST(0) when the compiler emits the call.
// Note: __cdecl decoration adds one leading underscore, so we export as
// ___ftol2_sse. The pragma below aliases it to __ftol2_sse for callers.
extern "C" long __cdecl __ftol2_sse()
{
    long result;
    __asm fistp dword ptr result
    return result;
}
#pragma comment(linker, "/export:__ftol2_sse=___ftol2_sse")

// roundf — not provided by Xbox CRT.
// Uses FPU to avoid triggering __ftol2_sse.
extern "C" float __cdecl roundf(float x)
{
    long result;
    __asm fld x
    __asm fistp result
    return (float)result;
}