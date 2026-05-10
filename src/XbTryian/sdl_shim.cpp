// sdl_shim.cpp
// Provides definitions for SDL functions that are declared in SDL.h but need
// xtl.h to implement. Kept in a separate translation unit so SDL.h itself
// doesn't pull xtl.h into every game source file.
//
// Phase note: SDL_Delay and SDL_GetTicks will become dead code once nortsong.cpp
// (Phase 4) replaces all call sites. This file can be removed at that point.

#include <xtl.h>
#include "SDL.h"

void SDL_Delay(Uint32 ms)
{
    Sleep(ms);
}

Uint32 SDL_GetTicks(void)
{
    return (Uint32)GetTickCount();
}