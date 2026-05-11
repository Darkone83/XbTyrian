// keyboard.cpp — Xbox input layer for OpenTyrian
//
// Replaces keyboard.c entirely. Drives keysactive[], keydown, newkey, and
// lastkey_scan from Xbox controller state via input.h/input.cpp.
//
// Design notes:
//   - service_SDL_events() is the single polling point called by ALL game code.
//     It calls PumpInput(), reads GetButtons(), and updates the keyboard state
//     arrays that every game loop checks.
//   - Left stick direction is injected as virtual d-pad presses so both stick
//     and d-pad work in menus and gameplay.
//   - joydown is defined here (not in joystick_stub.cpp) because we own the
//     controller input path that sets it.
//   - Mouse state variables are defined and permanently zeroed; no mouse on Xbox.
//   - ESCPressed is set when SDL_SCANCODE_ESCAPE is active (BTN_BACK by default).
//
// Button mapping: adjust s_map[] to change bindings. The current layout is:
//   D-pad / L-stick  → arrow keys   (menu nav and ship movement)
//   A                → Left Ctrl    (primary fire)
//   B                → Escape       (menu back / cancel)
//   X                → Left Shift   (speed / shield)
//   Y                → Tab          (weapon mode swap)
//   Start            → Return       (confirm)
//   Back             → Backspace    (secondary back — distinct from B/Escape)
//   L-trigger        → F3           (left weapon select)
//   R-trigger        → F4           (right weapon select)
//   Black            → F1           (help / options)
//   White            → F5           (pause)
//   L-thumb click    → F9
//   R-thumb click    → F10

#include "keyboard.h"
#include "input.h"

#include <string.h>   // memset

// =============================================================================
// Exported state — declared extern in keyboard.h
// =============================================================================

JE_boolean ESCPressed = false;
JE_boolean newkey = false;
JE_boolean newmouse = false;
JE_boolean keydown = false;
JE_boolean mousedown = false;
SDL_Scancode lastkey_scan = SDL_SCANCODE_UNKNOWN;
SDL_Keymod   lastkey_mod = KMOD_NONE;
Uint8        lastmouse_but = 0;
Sint32       lastmouse_x = 0;
Sint32       lastmouse_y = 0;
JE_boolean   mouse_pressed[3] = { false, false, false };
Sint32       mouse_x = 0;
Sint32       mouse_y = 0;
bool         windowHasFocus = true;   // always true on Xbox
Uint8        keysactive[SDL_NUM_SCANCODES];            // zeroed by init_keyboard()
bool         new_text = false;
char         last_text[SDL_TEXTINPUTEVENT_TEXT_SIZE] = { 0 };

// joydown: declared extern in joystick.h; defined here because we drive it.
// joystick_stub.cpp must NOT define it.
bool joydown = false;
bool ignore_joystick = false;  // declared extern in joystick.h

// mouseInactive: declared extern in mouse.h; defined in mouse_stub.cpp.
// Referenced here as extern to set it on key press (mirrors original behavior).
extern bool mouseInactive;

// =============================================================================
// Internal
// =============================================================================

// Threshold for stick-to-dpad injection. Higher than the GetSticks() deadzone
// (8000) so accidental small stick deflection doesn't trigger nav inputs.
#define STICK_KEY_THRESHOLD 16000

// Previous combined button+stick state for edge detection.
// Tracks WORD-sized BTN_* mask; stickBtns are ORed in per-frame.
static WORD s_prevButtons = 0;

// =============================================================================
// Button → scancode mapping table
// Adjust bindings here. Multiple buttons can map to the same scancode.
// =============================================================================

static const struct { WORD btn; SDL_Scancode scan; } s_map[] =
{
    // D-pad — navigation and gameplay movement
    { BTN_DPAD_UP,    SDL_SCANCODE_UP        },
    { BTN_DPAD_DOWN,  SDL_SCANCODE_DOWN      },
    { BTN_DPAD_LEFT,  SDL_SCANCODE_LEFT      },
    { BTN_DPAD_RIGHT, SDL_SCANCODE_RIGHT     },

    // Face buttons — match OpenTyrian's default KeySettings in config.cpp:
    // fire = SPACE, change fire = RETURN, left sidekick = LCTRL, right sidekick = LALT.
    // This keeps the controller as a keyboard compatibility layer instead of
    // bypassing the original button[] path in mainint.cpp.
    { BTN_A,          SDL_SCANCODE_SPACE     },  // primary fire / menu action
    { BTN_B,          SDL_SCANCODE_RETURN    },  // change fire / confirm
    { BTN_X,          SDL_SCANCODE_LCTRL     },  // left sidekick
    { BTN_Y,          SDL_SCANCODE_LALT      },  // right sidekick

    // Menu / system
    { BTN_START,      SDL_SCANCODE_RETURN    },  // confirm / change fire
    { BTN_BACK,       SDL_SCANCODE_ESCAPE    },  // back / in-game menu

    // Analog buttons
    { BTN_LTRIG,      SDL_SCANCODE_RETURN    },  // change fire
    { BTN_RTRIG,      SDL_SCANCODE_SPACE     },  // primary fire
    { BTN_BLACK,      SDL_SCANCODE_F1        },  // help
    { BTN_WHITE,      SDL_SCANCODE_P         },  // pause

    // Thumb clicks
    { BTN_LTHUMB,     SDL_SCANCODE_F9        },
    { BTN_RTHUMB,     SDL_SCANCODE_F10       },
};

#define S_MAP_COUNT  (sizeof(s_map) / sizeof(s_map[0]))

// =============================================================================
// JE_getInputName
//
// Display-only helper.  The controller still maps through keyboard scancodes;
// this only changes what Xbox users see in menus/prompts.
// =============================================================================

const char* JE_getInputName(SDL_Scancode scancode)
{
#ifdef _XBOX
    switch (scancode)
    {
    case SDL_SCANCODE_UP:      return "D-PAD UP";
    case SDL_SCANCODE_DOWN:    return "D-PAD DOWN";
    case SDL_SCANCODE_LEFT:    return "D-PAD LEFT";
    case SDL_SCANCODE_RIGHT:   return "D-PAD RIGHT";

    case SDL_SCANCODE_SPACE:   return "A / RT";
    case SDL_SCANCODE_RETURN:  return "B / START / LT";
    case SDL_SCANCODE_ESCAPE:  return "BACK";
    case SDL_SCANCODE_LCTRL:   return "X";
    case SDL_SCANCODE_LALT:    return "Y";
    case SDL_SCANCODE_P:       return "WHITE";
    case SDL_SCANCODE_F1:      return "BLACK";
    case SDL_SCANCODE_F9:      return "L-THUMB";
    case SDL_SCANCODE_F10:     return "R-THUMB";
    default:
        break;
    }
#endif

    return SDL_GetScancodeName(scancode);
}


// =============================================================================
// init_keyboard
// =============================================================================

void init_keyboard(void)
{
    InitInput();
    // Pump once immediately to detect controllers already connected at boot.
    // XInitDevices queues gamepad insertion events but InitInput only drains MUs.
    PumpInput();
    memset(keysactive, 0, sizeof(keysactive));
    newkey = false;
    newmouse = false;
    keydown = false;
    mousedown = false;
    joydown = false;
    ESCPressed = false;
    s_prevButtons = 0;
}

// =============================================================================
// service_SDL_events — the core polling function
//
// Called from ~72 sites across the game. Must be cheap on repeated calls
// within the same tick since several loops call it then immediately loop back.
// =============================================================================

void service_SDL_events(JE_boolean clear_new)
{
    if (clear_new)
    {
        newkey = false;
        newmouse = false;
        new_text = false;
    }

    PumpInput();

    WORD buttons = GetButtons();

    // Left stick → inject virtual d-pad flags.
    // These are ORed with the real d-pad so both input paths work simultaneously.
    int lx, ly, rx, ry;
    GetSticks(lx, ly, rx, ry);

    WORD stickBtns = 0;
    if (ly > STICK_KEY_THRESHOLD) stickBtns |= BTN_DPAD_UP;
    if (ly < -STICK_KEY_THRESHOLD) stickBtns |= BTN_DPAD_DOWN;
    if (lx < -STICK_KEY_THRESHOLD) stickBtns |= BTN_DPAD_LEFT;
    if (lx > STICK_KEY_THRESHOLD) stickBtns |= BTN_DPAD_RIGHT;

    WORD all = buttons | stickBtns;
    WORD pressed = all & ~s_prevButtons;   // rising edge this call

    // Update keysactive[] for every mapped scancode from the complete current
    // controller state, not just edge transitions.  This matters because several
    // Xbox buttons intentionally map to the same keyboard key now, e.g. A and
    // Right Trigger both map to SPACE.  If one is released while the other is
    // still held, the key must remain active so gameplay keeps firing.
    for (unsigned i = 0; i < S_MAP_COUNT; i++)
    {
        SDL_Scancode sc = s_map[i].scan;
        bool active = false;

        for (unsigned j = 0; j < S_MAP_COUNT; j++)
        {
            if (s_map[j].scan == sc && (all & s_map[j].btn))
            {
                active = true;
                break;
            }
        }

        keysactive[sc] = active ? 1 : 0;
    }

    // On any rising edge: set newkey + capture the first pressed scancode.
    // Matches SDL2 behavior where newkey fires once per key-down event.
    if (pressed)
    {
        for (unsigned i = 0; i < S_MAP_COUNT; i++)
        {
            if (pressed & s_map[i].btn)
            {
                newkey = true;
                lastkey_scan = s_map[i].scan;
                lastkey_mod = KMOD_NONE;
                mouseInactive = true;  // mirrors original keyboard.c behavior
                break;  // first button in table order wins
            }
        }
    }

    keydown = joydown = (all != 0);

    // ESCPressed convenience flag used by several game subsystems
    if (keysactive[SDL_SCANCODE_ESCAPE])
        ESCPressed = true;

    s_prevButtons = all;
}

// =============================================================================
// flush_events_buffer
// Called to discard any pending input (e.g. before a menu opens).
// =============================================================================

void flush_events_buffer(void)
{
    PumpInput();
    // Consume current state so next service_SDL_events sees no rising edges
    WORD buttons = GetButtons();
    int lx, ly, rx, ry;
    GetSticks(lx, ly, rx, ry);
    WORD stickBtns = 0;
    if (ly > STICK_KEY_THRESHOLD) stickBtns |= BTN_DPAD_UP;
    if (ly < -STICK_KEY_THRESHOLD) stickBtns |= BTN_DPAD_DOWN;
    if (lx < -STICK_KEY_THRESHOLD) stickBtns |= BTN_DPAD_LEFT;
    if (lx > STICK_KEY_THRESHOLD) stickBtns |= BTN_DPAD_RIGHT;
    s_prevButtons = buttons | stickBtns;

    memset(keysactive, 0, sizeof(keysactive));
    newkey = false;
    keydown = false;
    joydown = false;
    ESCPressed = false;
}

// =============================================================================
// wait_input / wait_noinput
// Spin with SDL_POLL_INTERVAL sleeps until the condition is met.
// The joystick parameter is treated as equivalent to keyboard since our
// controller IS the keyboard.
// =============================================================================

void wait_input(JE_boolean keyboard, JE_boolean mouse, JE_boolean joystick)
{
    service_SDL_events(false);
    while (!((keyboard && keydown) || (joystick && joydown)))
    {
        SDL_Delay(SDL_POLL_INTERVAL);
        service_SDL_events(false);
    }
}

void wait_noinput(JE_boolean keyboard, JE_boolean mouse, JE_boolean joystick)
{
    service_SDL_events(false);
    while ((keyboard && keydown) || (joystick && joydown))
    {
        SDL_Delay(SDL_POLL_INTERVAL);
        service_SDL_events(false);
    }
}

// =============================================================================
// Mouse stubs
// No mouse on Xbox. Variables are in mouse_stub.cpp; these are no-ops.
// =============================================================================

void mouseSetRelative(bool enable)
{
    (void)enable;
}

JE_word JE_mousePosition(JE_word* mouseX, JE_word* mouseY)
{
    *mouseX = 0;
    *mouseY = 0;
    return 0;
}

void mouseGetRelativePosition(Sint32* out_x, Sint32* out_y)
{
    *out_x = 0;
    *out_y = 0;
}

// =============================================================================
// JE_clearKeyboard
// =============================================================================

void JE_clearKeyboard(void)
{
    memset(keysactive, 0, sizeof(keysactive));
    keydown = false;
    joydown = false;
}

// =============================================================================
// sleep_game
// Declared in keyboard.h; no callers found in current source but defined for
// completeness. Can be used as a frame-rate courtesy sleep if needed.
// =============================================================================

void sleep_game(void)
{
    SDL_Delay(1);
}