// game_stubs.cpp — final state
//
// All game files are now ported. The only permanent stub is isNetworkGame
// since WITH_NETWORK is never defined on Xbox.
//
// If new LNK2001 errors appear after adding mainint.cpp or tyrian2.cpp,
// they indicate further unresolved symbols from those files' dependencies
// that need individual stubs added here.

#include <xtl.h>
#include "opentyr.h"

// =============================================================================
// network.h — permanent: no network support on Xbox
// =============================================================================

bool isNetworkGame = false;