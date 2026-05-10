#pragma once
// SDL_endian.h — Xbox compat stub
// Xbox is x86 little-endian. LE variants are identity; BE variants byte-swap.
// Used by file.h, sizebuf.c, sprite.c, config.c.

#include "SDL_types.h"

// Byte order constants — file.h uses these for conditional endian swapping
#define SDL_LIL_ENDIAN  1234
#define SDL_BIG_ENDIAN  4321
#define SDL_BYTEORDER   SDL_LIL_ENDIAN   // Xbox is x86 little-endian

// ---- Raw byte-swap helpers ----
static inline Uint16 SDL_Swap16(Uint16 x) {
    return (Uint16)((x << 8) | (x >> 8));
}
static inline Uint32 SDL_Swap32(Uint32 x) {
    return (x << 24) | ((x << 8) & 0x00FF0000u) |
        ((x >> 8) & 0x0000FF00u) | (x >> 24);
}
static inline Uint64 SDL_Swap64(Uint64 x) {
    return ((Uint64)SDL_Swap32((Uint32)(x & 0xFFFFFFFFu)) << 32) |
        (Uint64)SDL_Swap32((Uint32)(x >> 32));
}

// ---- Endian-aware accessors (Xbox = little-endian) ----
// SDL_SwapLE* = identity; SDL_SwapBE* = actual swap
#define SDL_SwapLE16(x)  ((Uint16)(x))
#define SDL_SwapLE32(x)  ((Uint32)(x))
#define SDL_SwapLE64(x)  ((Uint64)(x))
#define SDL_SwapBE16(x)  SDL_Swap16(x)
#define SDL_SwapBE32(x)  SDL_Swap32(x)
#define SDL_SwapBE64(x)  SDL_Swap64(x)