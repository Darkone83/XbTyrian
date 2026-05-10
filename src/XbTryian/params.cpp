// params.cpp — Xbox port of params.c
//
// On Xbox there are no command-line arguments (argc=0, argv=NULL).
// JE_paramCheck is preserved for link compatibility but the body is a no-op.
// All network/audio/joystick options must be set at compile time or via
// in-game menus in a future port phase.
//
// Variables exported for the rest of the game:
//   richMode, constantPlay, constantDie — all default false.

#include "params.h"

#include "loudness.h"
#include "opentyr.h"
#include "varz.h"

JE_boolean richMode = false;
JE_boolean constantPlay = false;
JE_boolean constantDie = false;

void JE_paramCheck(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    // No command-line arguments on Xbox.
    // Audio, joystick, and xmas flags retain their defaults.
}