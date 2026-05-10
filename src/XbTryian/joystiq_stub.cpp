// joystick_stub.cpp — Xbox port joystick stub
//
// The original joystick.c provides SDL joystick polling, config loading, and
// the push_joysticks_as_keyboard() mechanism. On Xbox all of that is replaced
// by keyboard.cpp (which drives keysactive[] directly from input.cpp).
//
// This file satisfies the linker by providing empty/null implementations of
// everything joystick.h declares. No SDL_Joystick handles are ever opened.
//
// Ownership notes:
//   - joydown        is defined in keyboard.cpp (we set it there alongside keydown)
//   - ignore_joystick is defined in keyboard.cpp
//   - joystick[]     is NULL; joysticks = 0
//   - push_key()     is a no-op (keyboard.cpp writes keysactive[] directly)

#include "joystick.h"

#include <stdlib.h>   // NULL

// =============================================================================
// Exported state
// =============================================================================

// joydown and ignore_joystick are defined in keyboard.cpp — do NOT define here.

int       joystick_repeat_delay = 300;
int       joysticks = 0;
Joystick* joystick = NULL;

// =============================================================================
// push_key
// In the original, synthesizes an SDL_KEYDOWN event via SDL_PushEvent.
// On Xbox, keyboard.cpp writes keysactive[] directly so this path is unused.
// =============================================================================

void push_key(SDL_Scancode key)
{
    (void)key;
}

// =============================================================================
// Axis / reduction helpers
// =============================================================================

int joystick_axis_reduce(int j, int value)
{
    (void)j; (void)value;
    return 0;
}

bool joystick_analog_angle(int j, float* angle)
{
    (void)j; (void)angle;
    return false;
}

// =============================================================================
// Poll functions — no-ops; keyboard.cpp handles polling via PumpInput()
// =============================================================================

void poll_joystick(int j) { (void)j; }
void poll_joysticks(void) {}
void push_joysticks_as_keyboard(void) {}

// =============================================================================
// Init / deinit — no-ops; InitInput() is called from init_keyboard()
// =============================================================================

void init_joysticks(void) {}
void deinit_joysticks(void) {}

// =============================================================================
// Config / assignment helpers — return safe defaults
// joystick[] is NULL so these should never be called at runtime, but we
// provide implementations in case config.c reaches them for joystick 0.
// =============================================================================

void reset_joystick_assignments(int j)
{
    (void)j;
}

bool load_joystick_assignments(Config* config, int j)
{
    (void)config; (void)j;
    return false;
}

bool save_joystick_assignments(Config* config, int j)
{
    (void)config; (void)j;
    return false;
}

void joystick_assignments_to_string(char* buffer, size_t buffer_len,
    const Joystick_assignment* assignments)
{
    (void)assignments;
    if (buffer && buffer_len) buffer[0] = '\0';
}

bool detect_joystick_assignment(int j, Joystick_assignment* assignment)
{
    (void)j; (void)assignment;
    return false;
}

bool joystick_assignment_cmp(const Joystick_assignment* a,
    const Joystick_assignment* b)
{
    (void)a; (void)b;
    return false;
}