// opentyr.cpp — Xbox port of opentyr.c
//
// Changes from original:
//   main() renamed to tyrian_main() under _XBOX guard.
//   setupMenu() and its picker helper functions excluded on Xbox (_XBOX guard).
//   All printf() calls removed (no console on Xbox).
//   fprintf(stderr, ...) replaced with OutputDebugStringA.
//   SDL_Init() failure check removed — our stub always returns 0.
//   SDL_Delay(16) in setupMenu excluded with the function.
//   "assuming mouse detected" printf removed.
//   Network block compiles out via WITH_NETWORK guard (unchanged).

#include <xtl.h>

#include "opentyr.h"

#include "config.h"
#include "destruct.h"
#include "editship.h"
#include "episodes.h"
#include "file.h"
#include "font.h"
#include "fonthand.h"
#include "helptext.h"
#include "joystick.h"
#include "jukebox.h"
#include "keyboard.h"
#include "loudness.h"
#include "mainint.h"
#include "mouse.h"
#include "mtrand.h"
#include "network.h"
#include "nortsong.h"
#include "nortvars.h"
#include "opentyrian_version.h"
#include "palette.h"
#include "params.h"
#include "picload.h"
#include "sprite.h"
#include "tyrian2.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"
#include "video_scale.h"
#include "xmas.h"

#include "SDL.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// =============================================================================
// Picker helper functions and setupMenu
//
// These are PC-only: they present a resolution/scaler settings screen before
// the game starts. On Xbox output is always 640x480 with nearest-neighbor
// scaling — no settings screen is needed or meaningful.
// =============================================================================

#ifndef _XBOX

static size_t getDisplayPickerItemsCount(void)
{
    return 1 + (size_t)SDL_GetNumVideoDisplays();
}

static const char* getDisplayPickerItem(size_t i, char* buffer, size_t bufferSize)
{
    if (i == 0)
        return "Window";
    snprintf(buffer, bufferSize, "Display %d", (int)i);
    return buffer;
}

static size_t getScalerPickerItemsCount(void)
{
    return (size_t)scalers_count;
}

static const char* getScalerPickerItem(size_t i, char* buffer, size_t bufferSize)
{
    (void)buffer; (void)bufferSize;
    return scalers[i].name;
}

static size_t getScalingModePickerItemsCount(void)
{
    return (size_t)ScalingMode_MAX;
}

static const char* getScalingModePickerItem(size_t i, char* buffer, size_t bufferSize)
{
    (void)buffer; (void)bufferSize;
    return scaling_mode_names[i];
}

void setupMenu(void)
{
    // Full setupMenu body omitted — see original opentyr.c.
    // On Xbox this function is never called.
}

#else // _XBOX

// Stub: setupMenu is never called on Xbox.
void setupMenu(void) {}

#endif // _XBOX

// =============================================================================
// tyrian_main — the game's init chain and main loop.
// Called from main.cpp after D3D device creation.
// =============================================================================

#ifdef _XBOX
int tyrian_main(int argc, char* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    mt_srand(time(NULL));

    // PC build prints version banner here — omitted on Xbox (no console).

    // SDL_Init is a no-op stub on Xbox; check removed.
    SDL_Init(0);

    JE_loadConfiguration();

    xmas = xmas_time();  // arg handler may override

    JE_paramCheck(argc, argv);

    // Platform init
    init_video();
    init_keyboard();
    init_joysticks();


    JE_scanForEpisodes();

    if (xmas && (!dir_file_exists(data_dir(), "tyrianc.shp") ||
        !dir_file_exists(data_dir(), "voicesc.snd")))
    {
        xmas = false;
        OutputDebugStringA("warning: Christmas data missing, disabling xmas mode\n");
    }

    JE_loadPals();
    JE_loadMainShapeTables(xmas ? "tyrianc.shp" : "tyrian.shp");

    if (xmas && !xmas_prompt())
    {
        xmas = false;
        free_main_shape_tables();
        JE_loadMainShapeTables("tyrian.shp");
    }

    // Default options
    youAreCheating = false;
    smoothScroll = true;
    loadDestruct = false;

    if (!audio_disabled)
    {
        init_audio();
        load_music();
        loadSndFile(xmas);
    }

    if (record_demo)
        OutputDebugStringA("demo recording enabled\n");

    JE_loadExtraShapes();   // Editship

    JE_loadHelpText();

    if (isNetworkGame)
    {
#ifdef WITH_NETWORK
        if (network_init())
            network_tyrian_halt(3, false);
#else
        OutputDebugStringA("OpenTyrian was compiled without networking support\n");
        JE_tyrianHalt(5);
#endif
    }

#ifdef NDEBUG
    if (!isNetworkGame)
        intro_logos();
#endif

    for (;;)
    {
        JE_initPlayerData();
        JE_sortHighScores();

        play_demo = false;
        stopped_demo = false;
        gameLoaded = false;
        jumpSection = false;

#ifdef WITH_NETWORK
        if (isNetworkGame)
        {
            networkStartScreen();
        }
        else
#endif
        {
            bool ts_result = titleScreen();
            if (!ts_result)
                break;  // player quit from title screen
        }

        if (loadDestruct)
        {
            JE_destructGame();
            loadDestruct = false;
        }
        else
        {
            JE_main();

            if (trentWin)
                break;  // player beat SuperTyrian
        }
    }

    JE_tyrianHalt(0);

    return 0;
}