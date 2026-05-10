// mouse_stub.cpp — Xbox port mouse stub
//
// No mouse on Xbox. This file defines all symbols declared in mouse.h so the
// game files that reference them link cleanly without any behavioral changes.
//
// mouseInactive is defined here because keyboard.cpp sets it on key press
// (mirroring the original keyboard.c behavior where it suppressed the cursor).

#include "mouse.h"

// =============================================================================
// Exported state — declared extern in mouse.h
// =============================================================================

bool     has_mouse = false;
bool     mouse_has_three_buttons = false;
bool     mouseInactive = true;   // starts true; keyboard.cpp sets it too
JE_byte  mouseCursor = 0;
JE_word  mouseX = 0;
JE_word  mouseY = 0;
JE_word  mouseButton = 0;
JE_word  mouseXB = 0;
JE_word  mouseYB = 0;

// =============================================================================
// Functions — declared in mouse.h; all no-ops on Xbox
// =============================================================================

void JE_mouseStart(void) {}
void JE_mouseStartFilter(Uint8 filter) { (void)filter; }
void JE_mouseReplace(void) {}