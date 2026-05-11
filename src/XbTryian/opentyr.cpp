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

// =============================================================================
// Xbox setup menu
//
// Called in two contexts:
//   1. From main.cpp before tyrian_main() — VGAScreen is NULL, D3D is up.
//      Uses raw D3D colored blocks for rendering.
//   2. From tyrian2.cpp (title screen Setup option) — VGAScreen is live,
//      fonts and palette are loaded. Uses font rendering.
//
// Settings are saved to E:\TYRIAN\xbtyrian.cfg on exit.
// =============================================================================

#include "xbox_config.h"
#include "video.h"
#include "vga256d.h"
#include "font.h"
#include "palette.h"
#include "keyboard.h"
#include "input.h"
#include "nortsong.h"


static JE_byte setupMenuBackdrop[320 * 200];
static Palette setupMenuBackdropPalette;
static bool setupMenuBackdropValid = false;

void xbox_setup_capture_backdrop(SDL_Surface* src, const void* palette)
{
    if (src == NULL || src->pixels == NULL || palette == NULL)
    {
        setupMenuBackdropValid = false;
        return;
    }

    const size_t bytes = (size_t)src->pitch * src->h;
    if (src->w != 320 || src->h != 200 || bytes > sizeof(setupMenuBackdrop))
    {
        setupMenuBackdropValid = false;
        return;
    }

    memcpy(setupMenuBackdrop, src->pixels, bytes);
    memcpy(setupMenuBackdropPalette, palette, sizeof(setupMenuBackdropPalette));
    setupMenuBackdropValid = true;
}

static void xbox_setup_restore_backdrop(void)
{
    if (!setupMenuBackdropValid || VGAScreen == NULL || VGAScreen->pixels == NULL)
    {
        fill_rectangle_xy(VGAScreen, 0, 0, 319, 199, 0);
        return;
    }

    const size_t bytes = (size_t)VGAScreen->pitch * VGAScreen->h;
    if (VGAScreen->w == 320 && VGAScreen->h == 200 && bytes <= sizeof(setupMenuBackdrop))
    {
        memcpy(VGAScreen->pixels, setupMenuBackdrop, bytes);
        set_palette(setupMenuBackdropPalette, 0, 255);
    }
    else
    {
        fill_rectangle_xy(VGAScreen, 0, 0, 319, 199, 0);
    }
}


// ---------------------------------------------------------------------------
// Internal: draw the menu using D3D clear blocks (pre-init context)
// ---------------------------------------------------------------------------

// Color scheme
#define COL_BG          0x00101018      // dark blue-grey
#define COL_TITLE       0x0080D0FF      // light blue
#define COL_ITEM        0x00C0C0C0      // light grey
#define COL_SELECTED    0x00FFFF40      // yellow
#define COL_DISABLED    0x00505060      // dim grey
#define COL_BAR         0x00204060      // menu bar blue
#define COL_BAR_SEL     0x00306090      // selected bar

static void d3d_fill(int x, int y, int w, int h, DWORD color)
{
    D3DRECT r;
    r.x1 = x; r.y1 = y; r.x2 = x + w; r.y2 = y + h;
    D3DDevice_Clear(1, &r, D3DCLEAR_TARGET, color, 1.0f, 0);
}

static void draw_setup_menu_d3d(XbConfig* cfg, int sel, int bbW, int bbH)
{
    // Background
    D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET, COL_BG, 1.0f, 0);

    // Title bar
    int barH = bbH / 12;
    d3d_fill(0, 0, bbW, barH, 0x00002040);

    // Title text approximation — colored blocks spelling out menu sections
    // (Font rendering not available in this context)
    // Draw a cyan accent line under title area
    d3d_fill(0, barH, bbW, 2, COL_TITLE);

    // Menu items
    static const char* const videoLabels[] = { "Auto", "480p", "720p", "", "480i" };
    static const char* const aspectLabels[] = { "4:3 Pillarbox", "16:9 Stretch", "Pixel Perfect" };
    static const char* const scanLabels[] = { "Off", "Light", "Medium", "Heavy" };
    static const char* const filterLabels[] = { "Sharp", "Smooth" };
    static const char* const brightnessLabels[] = { "Normal", "Bright" };
    static const char* const resetLabels[] = { "No", "Apply" };

    // Each row: label block + value blocks
    int rowH = bbH / 8;
    int startY = barH + bbH / 16;
    int labelW = bbW / 3;
    int indent = bbW / 16;

    struct MenuItem
    {
        const char* const* labels;
        int count;
        int* value;
    } items[6] = {
        { videoLabels,       5, (int*)&cfg->videoMode  },
        { aspectLabels,      2, (int*)&cfg->aspectMode },
        { scanLabels,        4, (int*)&cfg->scanlines  },
        { filterLabels,      2, (int*)&cfg->textureFilter },
        { brightnessLabels,  2, (int*)&cfg->brightness },
        { resetLabels,       2, NULL },
    };

    for (int i = 0; i < 6; i++)
    {
        int y = startY + i * (rowH + rowH / 4);
        bool cur = (sel == i);

        // Row highlight
        if (cur)
            d3d_fill(0, y - 2, bbW, rowH + 4, COL_BAR_SEL);
        else
            d3d_fill(0, y - 2, bbW, rowH + 4, COL_BAR);

        // Label color block
        DWORD lc = cur ? COL_TITLE : COL_ITEM;
        d3d_fill(indent, y + rowH / 4, labelW - indent * 2, rowH / 2, lc);

        // Value option blocks
        int optW = (bbW - labelW - indent * 2) / items[i].count - 4;
        for (int v = 0; v < items[i].count; v++)
        {
            int ox = labelW + indent + v * (optW + 4);
            DWORD vc;
            if (items[i].value != NULL && v == *items[i].value)
                vc = COL_SELECTED;
            else if (i == 0 && items[i].value != NULL && (v == (int)XB_VIDEO_1080I || (v > 0 && !xb_video_mode_available((XbVideoMode)v))))
                vc = COL_DISABLED;
            else
                vc = COL_ITEM;
            d3d_fill(ox, y + rowH / 6, optW, rowH * 2 / 3, vc);

            // Thin accent for current value
            if (items[i].value != NULL && v == *items[i].value)
                d3d_fill(ox, y + rowH - rowH / 6, optW, rowH / 6, 0x00FF8800);
        }
    }

    // Footer bar
    int footY = bbH - barH;
    d3d_fill(0, footY, bbW, 2, COL_TITLE);
    d3d_fill(0, footY + 2, bbW, barH - 2, 0x00001828);

    // Save indicator block (bottom right)
    d3d_fill(bbW - bbW / 6, footY + 4, bbW / 7, barH - 8, 0x00008040);

    D3DDevice_Swap(0);
}

// ---------------------------------------------------------------------------
// Internal: draw using font rendering (in-game context)
// ---------------------------------------------------------------------------

static void draw_setup_menu_vga(XbConfig* cfg, int sel)
{
    static const char* const rowNames[] =
    {
        "VIDEO OUTPUT",
        "SCALE MODE",
        "SCANLINES",
        "TEXTURE FILTER",
        "BRIGHTNESS",
        "RESET VIDEO"
    };

    /*
     * Video enum order is:
     *   AUTO=0, 480p=1, 720p=2, 1080i=3, 480i=4
     * Keep this array enum-indexed so the label always matches cfg.videoMode.
     * 1080i is intentionally hidden/skipped by the menu logic below.
     */
    static const char* const videoOpts[] = { "AUTO", "480p", "720p", "", "480i" };
    static const char* const aspectOpts[] = { "< 4:3 PILLARBOX >", "< 16:9 STRETCH  >", "< PIXEL PERFECT >", "< PIXEL FILL    >" };
    static const char* const scanOpts[] = { "< OFF    >", "< LIGHT  >", "< MEDIUM >", "< HEAVY  >" };
    static const char* const filterOpts[] = { "< SHARP  >", "< SMOOTH >" };
    static const char* const brightOpts[] = { "< NORMAL >", "< BRIGHT >" };
    static const char* const resetOpts[] = { "< APPLY DEFAULTS >" };

    const char* const* opts[6] =
    {
        videoOpts,
        aspectOpts,
        scanOpts,
        filterOpts,
        brightOpts,
        resetOpts
    };

    int* vals[6] =
    {
        (int*)&cfg->videoMode,
        (int*)&cfg->aspectMode,
        (int*)&cfg->scanlines,
        (int*)&cfg->textureFilter,
        (int*)&cfg->brightness,
        NULL
    };

    /*
     * Title-safe settings backdrop.
     *
     * The title screen already has the correct picture and palette loaded.
     * Capture that frame in tyrian2.cpp before fade_black(), then restore and
     * shade it here.  Do not call JE_loadPic() from the settings menu.
     */
    xbox_setup_restore_backdrop();
    JE_barShade(VGAScreen, 0, 0, 319, 199);
    JE_barShade(VGAScreen, 0, 0, 319, 199);

    // Clean menu panel.  The captured title background remains visible at the
    // edges while the center stays readable.
    fill_rectangle_xy(VGAScreen, 36, 8, 283, 183, 0);
    JE_rectangle(VGAScreen, 36, 8, 283, 183, 5);
    JE_rectangle(VGAScreen, 38, 10, 281, 181, 1);

    // Title backing.
    fill_rectangle_xy(VGAScreen, 58, 10, 262, 25, 0);
    JE_rectangle(VGAScreen, 58, 10, 262, 25, 1);

    draw_font_hv(VGAScreen, 160, 13, "XBTYRIAN SETTINGS", normal_font, centered, 15, 0);
    fill_rectangle_xy(VGAScreen, 34, 28, 285, 29, 5);

    const int rowY[6] = { 36, 59, 82, 105, 128, 151 };

    for (int i = 0; i < 6; ++i)
    {
        fill_rectangle_xy(VGAScreen, 46, rowY[i] - 2, 273, rowY[i] + 17, 0);
        JE_rectangle(VGAScreen, 46, rowY[i] - 2, 273, rowY[i] + 17, 1);
    }

    for (int i = 0; i < 6; i++)
    {
        bool cur = (sel == i);

        if (cur)
        {
            fill_rectangle_xy(VGAScreen, 44, rowY[i] - 3, 275, rowY[i] + 18, 1);
            JE_rectangle(VGAScreen, 44, rowY[i] - 3, 275, rowY[i] + 18, 5);
        }

        draw_font_hv(VGAScreen, 80, rowY[i], rowNames[i], small_font, centered, cur ? 15 : 7, cur ? 2 : 0);

        if (i == 0)
        {
            int v = *vals[i];
            Uint8 hue = (v != (int)XB_VIDEO_1080I && (xb_video_mode_available((XbVideoMode)v) || v == 0)) ? (cur ? 14 : 5) : 2;
            char buf[40];
            wsprintfA(buf, "< %s >", videoOpts[v]);
            draw_font_hv(VGAScreen, 216, rowY[i], buf, normal_font, centered, hue, 0);
            draw_font_hv(VGAScreen, 216, rowY[i] + 10, "(restart)", small_font, centered, 3, -2);
        }
        else if (i == 5)
        {
            draw_font_hv(VGAScreen, 216, rowY[i] + 2, resetOpts[0], normal_font, centered, cur ? 14 : 5, 0);
        }
        else
        {
            draw_font_hv(VGAScreen, 216, rowY[i] + 2, opts[i][*vals[i]], normal_font, centered, cur ? 14 : 5, 0);
        }
    }

    // Footer hints. Keep this uncluttered and Xbox-facing.
    fill_rectangle_xy(VGAScreen, 36, 187, 283, 188, 5);
    draw_font_hv(VGAScreen, 160, 191, "A Change   Y Save   B Back   \x1B/\x1A Row", small_font, centered, 5, -2);

    JE_showVGA();
}

// ---------------------------------------------------------------------------
// setupMenu — entry point
// ---------------------------------------------------------------------------

void setupMenu(void)
{
    XbConfig cfg;
    xb_config_load(&cfg);

    if (cfg.videoMode == XB_VIDEO_1080I)
        cfg.videoMode = XB_VIDEO_AUTO;

    int sel = 0;
    const int numRows = 6;

    // Detect context: VGAScreen initialized = in-game, else pre-init
    bool inGame = true;  // always in-game context now

    // In-game: init_video already ran, we have backbuffer dims from video.h
    // Pre-init: device was created at 1280x720 by main.cpp
    int bbW = inGame ? 320 : 1280;
    int bbH = inGame ? 200 : 720;

    // Input state — track prev to detect edges
    WORD prevButtons = 0;
    bool running = true;

    // The VGA draw path restores the captured title palette each frame.
    // If no backdrop was captured, it falls back to a clean black screen.

    while (running)
    {
        // Render
        if (inGame)
            draw_setup_menu_vga(&cfg, sel);
        else
            draw_setup_menu_d3d(&cfg, sel, bbW, bbH);

        // Poll input
        PumpInput();
        WORD btn = GetButtons();
        WORD pressed = btn & ~prevButtons;
        prevButtons = btn;

        // Navigation
        if (pressed & BTN_DPAD_UP)
            sel = (sel + numRows - 1) % numRows;
        if (pressed & BTN_DPAD_DOWN)
            sel = (sel + 1) % numRows;

        // Value change for current row
        int* vals[6] = { (int*)&cfg.videoMode, (int*)&cfg.aspectMode, (int*)&cfg.scanlines, (int*)&cfg.textureFilter, (int*)&cfg.brightness, NULL };
        int  maxs[6] = { (int)XB_VIDEO_480I, (int)XB_ASPECT_PIXEL_FILL, (int)XB_SCANLINES_HEAVY, (int)XB_TEXTURE_FILTER_SMOOTH, (int)XB_BRIGHTNESS_BRIGHT, 0 };

        bool changed = false;
        if (pressed & (BTN_DPAD_RIGHT | BTN_A | BTN_START))
        {
            if (sel == 5)
            {
                XbVideoMode keepVideo = cfg.videoMode;
                XbConfig def = XB_CONFIG_DEFAULT;

                // Reset display-side settings immediately, but keep the user's
                // selected output mode unless they change/save that row.
                cfg = def;
                cfg.videoMode = keepVideo;
                changed = true;
            }
            else
            {
                int v = *vals[sel];
                do { v = (v + 1) % (maxs[sel] + 1); } while (sel == 0 && (v == (int)XB_VIDEO_1080I || (v != 0 && !xb_video_mode_available((XbVideoMode)v))));
                *vals[sel] = v;
                changed = true;
            }
        }
        if (pressed & BTN_DPAD_LEFT)
        {
            if (sel != 5)
            {
                int v = *vals[sel];
                do { v = (v + maxs[sel]) % (maxs[sel] + 1); } while (sel == 0 && (v == (int)XB_VIDEO_1080I || (v != 0 && !xb_video_mode_available((XbVideoMode)v))));
                *vals[sel] = v;
                changed = true;
            }
        }

        // Apply display-side settings immediately.  Video output still applies
        // on next boot after save/restart.
        if (changed && sel != 0)
            video_apply_settings((int)cfg.aspectMode, (int)cfg.scanlines, (int)cfg.textureFilter, (int)cfg.brightness);

        // Y = save and exit
        if (pressed & BTN_Y)
        {
            xb_config_save(&cfg);
            running = false;
        }

        // B or Back = exit without saving
        if (pressed & (BTN_B | BTN_BACK))
            running = false;

        Sleep(16);
    }

    // Fade back to black so the title screen restart looks clean
    fade_black(10);
}

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