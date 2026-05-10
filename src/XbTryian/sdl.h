#pragma once
// SDL.h — Xbox compat stub (Phase 1)
//
// Replaces SDL2 for the OpenTyrian Xbox port. Provides:
//   - All types/structs the game files reference
//   - Working inline implementations for SDL_FillRect, SDL_MapRGB, SDL_strlcpy
//   - Declarations for functions provided by sdl_shim.cpp (SDL_Delay, SDL_GetTicks)
//   - Compile-only stubs for SDL video/audio calls that Phase 3+ will replace
//
// All files are compiled as C++ (/TP). C89 compat in this header is not required.

#include "SDL_types.h"
#include "SDL_endian.h"
#include <string.h>  // memset, strncpy, strlen, strcmp

// =============================================================================
// Structures
// =============================================================================

// SDL_PixelFormat — game uses only BitsPerPixel; palette.c passes it to SDL_MapRGB.
typedef struct SDL_PixelFormat {
    int BitsPerPixel;
} SDL_PixelFormat;

// SDL_Surface — must match what the game writes to directly.
// Game surfaces are always 8bpp indexed at 320x200. pixels is a flat Uint8 array.
typedef struct SDL_Surface {
    Uint8* pixels;   // row-major pixel data; game writes here without locking
    int              pitch;    // bytes per scanline (= w for 8bpp, no padding)
    int              w, h;     // width/height in pixels
    SDL_PixelFormat* format;   // points to a static instance in video.cpp (Phase 3)
} SDL_Surface;

// Opaque render-pipeline types — video.cpp (Phase 3) owns these.
typedef void SDL_Window;
typedef void SDL_Renderer;
typedef void SDL_Texture;

// Opaque joystick handle — joystick_stub.cpp always leaves it NULL.
typedef void SDL_Joystick;

// SDL_Rect — used by vga256d.h, video.h, vga256d.c
typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;

// SDL_Color — used by palette.h/palette.c extensively
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;

// =============================================================================
// SDL_Scancode
// Values match SDL2 (USB HID usage page) so config files from PC builds round-trip.
// =============================================================================

typedef enum SDL_Scancode
{
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4,
    SDL_SCANCODE_B = 5,
    SDL_SCANCODE_C = 6,
    SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9,
    SDL_SCANCODE_G = 10,
    SDL_SCANCODE_H = 11,
    SDL_SCANCODE_I = 12,
    SDL_SCANCODE_J = 13,
    SDL_SCANCODE_K = 14,
    SDL_SCANCODE_L = 15,
    SDL_SCANCODE_M = 16,
    SDL_SCANCODE_N = 17,
    SDL_SCANCODE_O = 18,
    SDL_SCANCODE_P = 19,
    SDL_SCANCODE_Q = 20,
    SDL_SCANCODE_R = 21,
    SDL_SCANCODE_S = 22,
    SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24,
    SDL_SCANCODE_V = 25,
    SDL_SCANCODE_W = 26,
    SDL_SCANCODE_X = 27,
    SDL_SCANCODE_Y = 28,
    SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30,
    SDL_SCANCODE_2 = 31,
    SDL_SCANCODE_3 = 32,
    SDL_SCANCODE_4 = 33,
    SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35,
    SDL_SCANCODE_7 = 36,
    SDL_SCANCODE_8 = 37,
    SDL_SCANCODE_9 = 38,
    SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40,
    SDL_SCANCODE_ESCAPE = 41,
    SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_F1 = 58,
    SDL_SCANCODE_F2 = 59,
    SDL_SCANCODE_F3 = 60,
    SDL_SCANCODE_F4 = 61,
    SDL_SCANCODE_F5 = 62,
    SDL_SCANCODE_F6 = 63,
    SDL_SCANCODE_F7 = 64,
    SDL_SCANCODE_F8 = 65,
    SDL_SCANCODE_F9 = 66,
    SDL_SCANCODE_F10 = 67,
    SDL_SCANCODE_F11 = 68,
    SDL_SCANCODE_F12 = 69,
    SDL_SCANCODE_DELETE = 76,
    SDL_SCANCODE_RIGHT = 79,
    SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_LCTRL = 224,
    SDL_SCANCODE_LSHIFT = 225,
    SDL_SCANCODE_LALT = 226,
    SDL_SCANCODE_LGUI = 227,
    SDL_SCANCODE_RCTRL = 228,
    SDL_SCANCODE_RSHIFT = 229,
    SDL_SCANCODE_RALT = 230,
    SDL_SCANCODE_RGUI = 231,

    // Punctuation / symbol keys (USB HID values matching SDL2)
    SDL_SCANCODE_MINUS = 45,
    SDL_SCANCODE_EQUALS = 46,
    SDL_SCANCODE_LEFTBRACKET = 47,
    SDL_SCANCODE_RIGHTBRACKET = 48,
    SDL_SCANCODE_BACKSLASH = 49,
    SDL_SCANCODE_SEMICOLON = 51,
    SDL_SCANCODE_GRAVE = 53,
    SDL_SCANCODE_COMMA = 54,
    SDL_SCANCODE_PERIOD = 55,
    SDL_SCANCODE_SLASH = 56,
    SDL_SCANCODE_CAPSLOCK = 57,

    // Navigation cluster
    SDL_SCANCODE_INSERT = 73,
    SDL_SCANCODE_HOME = 74,
    SDL_SCANCODE_PAGEUP = 75,
    SDL_SCANCODE_END = 77,
    SDL_SCANCODE_PAGEDOWN = 78,
    SDL_SCANCODE_SCROLLLOCK = 71,
    SDL_SCANCODE_NUMLOCKCLEAR = 83,

    // Keypad
    SDL_SCANCODE_KP_1 = 89,
    SDL_SCANCODE_KP_2 = 90,
    SDL_SCANCODE_KP_3 = 91,
    SDL_SCANCODE_KP_4 = 92,
    SDL_SCANCODE_KP_5 = 93,
    SDL_SCANCODE_KP_6 = 94,
    SDL_SCANCODE_KP_7 = 95,
    SDL_SCANCODE_KP_8 = 96,
    SDL_SCANCODE_KP_9 = 97,
    SDL_SCANCODE_KP_0 = 98,
    SDL_SCANCODE_KP_MINUS = 86,
    SDL_SCANCODE_KP_PLUS = 87,
    SDL_SCANCODE_KP_ENTER = 88,

    SDL_NUM_SCANCODES = 512
} SDL_Scancode;

// =============================================================================
// SDL_Keymod
// Used by keyboard.h (lastkey_mod), starlib.c, menus.c.
// On Xbox we always report KMOD_NONE; modifier synthesis is future work.
// =============================================================================

typedef int SDL_Keymod;
enum
{
    KMOD_NONE = 0x0000,
    KMOD_LSHIFT = 0x0001,
    KMOD_RSHIFT = 0x0002,
    KMOD_LCTRL = 0x0040,
    KMOD_RCTRL = 0x0080,
    KMOD_LALT = 0x0100,
    KMOD_RALT = 0x0200,
    KMOD_LGUI = 0x0400,
    KMOD_RGUI = 0x0800,
    KMOD_NUM = 0x1000,
    KMOD_CAPS = 0x2000,
    KMOD_MODE = 0x4000,
    KMOD_CTRL = (KMOD_LCTRL | KMOD_RCTRL),
    KMOD_SHIFT = (KMOD_LSHIFT | KMOD_RSHIFT),
    KMOD_ALT = (KMOD_LALT | KMOD_RALT),
    KMOD_GUI = (KMOD_LGUI | KMOD_RGUI),
};

// menus.c: SDL_GetModState() & KMOD_SHIFT — always no modifier on Xbox
static inline SDL_Keymod SDL_GetModState(void) { return KMOD_NONE; }

// =============================================================================
// Text input constants
// =============================================================================

#define SDL_TEXTINPUTEVENT_TEXT_SIZE 32

// =============================================================================
// SDL_Surface helpers (used by game code we keep)
// =============================================================================

// SDL_MUSTLOCK — our software surfaces never need locking
#define SDL_MUSTLOCK(s) 0

// SDL_FillRect — used by vga256d.c for rectangle fills on 8bpp indexed surfaces.
// color is an 8-bit palette index cast to Uint32.
static inline int SDL_FillRect(SDL_Surface* dst, const SDL_Rect* rect, Uint32 color)
{
    if (!dst || !dst->pixels) return -1;
    int x0 = rect ? rect->x : 0;
    int y0 = rect ? rect->y : 0;
    int w = rect ? rect->w : dst->w;
    int h = rect ? rect->h : dst->h;
    for (int row = y0; row < y0 + h; row++)
        memset((Uint8*)dst->pixels + row * dst->pitch + x0, (Uint8)color, w);
    return 0;
}

// SDL_MapRGB — used by palette.c to build rgb_palette[].
// Returns D3DFMT_X8R8G8B8 packed value matching the Phase 3 D3D8 texture format,
// so rgb_palette[] can be used directly by video.cpp without re-packing.
static inline Uint32 SDL_MapRGB(SDL_PixelFormat* fmt, Uint8 r, Uint8 g, Uint8 b)
{
    (void)fmt;
    return ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
}

// SDL_strlcpy — used in keyboard.c (original); kept as a utility
static inline size_t SDL_strlcpy(char* dst, const char* src, size_t maxlen)
{
    size_t srclen = strlen(src);
    if (maxlen) {
        size_t copy = srclen < maxlen - 1 ? srclen : maxlen - 1;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return srclen;
}

// =============================================================================
// SDL_GetScancodeName / SDL_GetScancodeFromName
// Used by game_menu.c, config.c, destruct.c for key binding display/serialization.
// We return the same strings SDL2 would for the scancodes we actually map.
// Unrecognised scancodes return "" / SDL_SCANCODE_UNKNOWN so config files fail
// gracefully rather than crashing.
// =============================================================================

static inline const char* SDL_GetScancodeName(SDL_Scancode sc)
{
    switch (sc)
    {
    case SDL_SCANCODE_UP:        return "Up";
    case SDL_SCANCODE_DOWN:      return "Down";
    case SDL_SCANCODE_LEFT:      return "Left";
    case SDL_SCANCODE_RIGHT:     return "Right";
    case SDL_SCANCODE_RETURN:    return "Return";
    case SDL_SCANCODE_ESCAPE:    return "Escape";
    case SDL_SCANCODE_BACKSPACE: return "Backspace";
    case SDL_SCANCODE_TAB:       return "Tab";
    case SDL_SCANCODE_SPACE:     return "Space";
    case SDL_SCANCODE_LCTRL:     return "Left Ctrl";
    case SDL_SCANCODE_LSHIFT:    return "Left Shift";
    case SDL_SCANCODE_LALT:      return "Left Alt";
    case SDL_SCANCODE_F1:        return "F1";
    case SDL_SCANCODE_F2:        return "F2";
    case SDL_SCANCODE_F3:        return "F3";
    case SDL_SCANCODE_F4:        return "F4";
    case SDL_SCANCODE_F5:        return "F5";
    case SDL_SCANCODE_F6:        return "F6";
    case SDL_SCANCODE_F7:        return "F7";
    case SDL_SCANCODE_F8:        return "F8";
    case SDL_SCANCODE_F9:        return "F9";
    case SDL_SCANCODE_F10:       return "F10";
    default:                     return "";
    }
}

static inline SDL_Scancode SDL_GetScancodeFromName(const char* name)
{
    if (!name || !*name) return SDL_SCANCODE_UNKNOWN;
    if (strcmp(name, "Up") == 0) return SDL_SCANCODE_UP;
    if (strcmp(name, "Down") == 0) return SDL_SCANCODE_DOWN;
    if (strcmp(name, "Left") == 0) return SDL_SCANCODE_LEFT;
    if (strcmp(name, "Right") == 0) return SDL_SCANCODE_RIGHT;
    if (strcmp(name, "Return") == 0) return SDL_SCANCODE_RETURN;
    if (strcmp(name, "Escape") == 0) return SDL_SCANCODE_ESCAPE;
    if (strcmp(name, "Backspace") == 0) return SDL_SCANCODE_BACKSPACE;
    if (strcmp(name, "Tab") == 0) return SDL_SCANCODE_TAB;
    if (strcmp(name, "Space") == 0) return SDL_SCANCODE_SPACE;
    if (strcmp(name, "Left Ctrl") == 0) return SDL_SCANCODE_LCTRL;
    if (strcmp(name, "Left Shift") == 0) return SDL_SCANCODE_LSHIFT;
    if (strcmp(name, "Left Alt") == 0) return SDL_SCANCODE_LALT;
    if (strcmp(name, "F1") == 0) return SDL_SCANCODE_F1;
    if (strcmp(name, "F2") == 0) return SDL_SCANCODE_F2;
    if (strcmp(name, "F3") == 0) return SDL_SCANCODE_F3;
    if (strcmp(name, "F4") == 0) return SDL_SCANCODE_F4;
    if (strcmp(name, "F5") == 0) return SDL_SCANCODE_F5;
    if (strcmp(name, "F6") == 0) return SDL_SCANCODE_F6;
    if (strcmp(name, "F7") == 0) return SDL_SCANCODE_F7;
    if (strcmp(name, "F8") == 0) return SDL_SCANCODE_F8;
    if (strcmp(name, "F9") == 0) return SDL_SCANCODE_F9;
    if (strcmp(name, "F10") == 0) return SDL_SCANCODE_F10;
    return SDL_SCANCODE_UNKNOWN;
}

// =============================================================================
// SDL_Delay / SDL_GetTicks
// Declared here; defined in sdl_shim.cpp (which includes xtl.h).
// Phase 4 (nortsong.cpp) will remove the SDL_Delay/SDL_GetTicks calls from the
// game files entirely; these exist only to keep nortsong.c compiling until then.
// =============================================================================

void    SDL_Delay(Uint32 ms);
Uint32  SDL_GetTicks(void);

// =============================================================================
// SDL_SetHint — no-op on Xbox
// =============================================================================

static inline int SDL_SetHint(const char* name, const char* value)
{
    (void)name; (void)value; return 0;
}

// =============================================================================
// Miscellaneous
// =============================================================================

static inline const char* SDL_GetError(void) { return ""; }
static inline void         SDL_Quit(void) {}
static inline int    SDL_Init(Uint32 f) { (void)f; return 0; }  // always succeeds
static inline Uint32 SDL_WasInit(Uint32 f) { (void)f; return 0; }

// Display enumeration — Xbox always has exactly one display.
// getDisplayPickerItemsCount adds 1, so returning 0 gives one total entry.
static inline int SDL_GetNumVideoDisplays(void) { return 0; }

// Mouse button constants — newmouse is always false on Xbox so these
// code paths never execute at runtime, but they must compile.
enum
{
    SDL_BUTTON_LEFT = 1,
    SDL_BUTTON_MIDDLE = 2,
    SDL_BUTTON_RIGHT = 3,
};

// snprintf — excluded: only used inside #ifndef _XBOX blocks (setupMenu)
// If needed in future, use _snprintf directly.

// SDL_VERSION_ATLEAST — always false; blocks SDL2-version-gated code paths cleanly
#define SDL_VERSION_ATLEAST(x, y, z) 0

// =============================================================================
// Phase 3+ compile stubs
// These exist so the SDL-calling files we haven't replaced yet still compile.
// They will be removed/replaced when their owning file is ported.
// =============================================================================

// [REPLACE in Phase 3 — video.cpp owns these]
static inline SDL_Surface* SDL_CreateRGBSurface(Uint32 f, int w, int h, int d,
    Uint32 rm, Uint32 gm, Uint32 bm, Uint32 am)
{
    (void)f; (void)d; (void)rm; (void)gm; (void)bm; (void)am; (void)w; (void)h; return NULL;
}
static inline void           SDL_FreeSurface(SDL_Surface* s) { (void)s; }
static inline SDL_PixelFormat* SDL_AllocFormat(Uint32 pf) { (void)pf; return NULL; }
static inline void             SDL_FreeFormat(SDL_PixelFormat* f) { (void)f; }
static inline int  SDL_LockTexture(SDL_Texture* t, const SDL_Rect* r, void** p, int* pitch)
{
    (void)t; (void)r; (void)p; (void)pitch; return -1;
}
static inline void SDL_UnlockTexture(SDL_Texture* t) { (void)t; }
static inline int  SDL_QueryTexture(SDL_Texture* t, Uint32* fmt, int* a, int* w, int* h)
{
    (void)t; (void)fmt; (void)a; (void)w; (void)h; return -1;
}

// [REPLACE in Phase 5 — loudness.cpp owns these]
typedef Uint32 SDL_AudioDeviceID;
typedef Uint16 SDL_AudioFormat;
#define AUDIO_S16SYS                     0x8010
#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 0x01
#define SDL_AUDIO_ALLOW_SAMPLES_CHANGE   0x08
#define SDL_INIT_VIDEO                   0x00000020u
#define SDL_INIT_AUDIO                   0x00000010u

typedef struct SDL_AudioSpec {
    int             freq;
    SDL_AudioFormat format;
    Uint8           channels;
    Uint16          samples;
    void          (*callback)(void* userdata, Uint8* stream, int len);
    void* userdata;
} SDL_AudioSpec;

static inline int SDL_InitSubSystem(Uint32 f)
{
    (void)f; return 0;
}
static inline void SDL_QuitSubSystem(Uint32 f)
{
    (void)f;
}
static inline SDL_AudioDeviceID SDL_OpenAudioDevice(const char* d, int c,
    const SDL_AudioSpec* des, SDL_AudioSpec* got, int allowed)
{
    (void)d; (void)c; (void)des; (void)got; (void)allowed; return 0;
}
static inline void SDL_PauseAudioDevice(SDL_AudioDeviceID dev, int p)
{
    (void)dev; (void)p;
}
static inline void SDL_LockAudioDevice(SDL_AudioDeviceID dev) { (void)dev; }
static inline void SDL_UnlockAudioDevice(SDL_AudioDeviceID dev) { (void)dev; }