// xbox_config.cpp — Xbox-specific video/display settings

#include <xtl.h>
#include "xbox_config.h"
#include "file.h"
#include <string.h>

// =============================================================================
// Load / Save
// =============================================================================

void xb_config_load(XbConfig* cfg)
{
    XbConfig def = XB_CONFIG_DEFAULT;
    *cfg = def;

    FILE* f = dir_fopen(data_dir(), "xbtyrian.cfg", "rb");
    if (!f)
        return;

    if (fread(cfg, sizeof(XbConfig), 1, f) != 1)
        *cfg = def;

    fclose(f);

    // Validate ranges — corrupt file protection
    if (cfg->videoMode > XB_VIDEO_480I)   cfg->videoMode = XB_VIDEO_AUTO;
    if (cfg->aspectMode > XB_ASPECT_PIXEL_FILL) cfg->aspectMode = XB_ASPECT_4_3;
    if (cfg->scanlines > XB_SCANLINES_HEAVY) cfg->scanlines = XB_SCANLINES_OFF;
    if (cfg->textureFilter > XB_TEXTURE_FILTER_SMOOTH) cfg->textureFilter = XB_TEXTURE_FILTER_SHARP;
    if (cfg->brightness > XB_BRIGHTNESS_BRIGHT) cfg->brightness = XB_BRIGHTNESS_NORMAL;
}

void xb_config_save(const XbConfig* cfg)
{
    FILE* f = dir_fopen(data_dir(), "xbtyrian.cfg", "wb");
    if (!f)
    {
        OutputDebugStringA("xb_config_save: failed to open xbtyrian.cfg\n");
        return;
    }
    fwrite(cfg, sizeof(XbConfig), 1, f);
    fclose(f);
}

// =============================================================================
// Video mode queries
// =============================================================================

int xb_video_mode_available(XbVideoMode mode)
{
    DWORD flags = XGetVideoFlags();

    switch (mode)
    {
    case XB_VIDEO_480I:
        return 1;  // always available (base SD output)

    case XB_VIDEO_480P:
        return (flags & XC_VIDEO_FLAGS_HDTV_480p) != 0;

    case XB_VIDEO_720P:
        return (flags & XC_VIDEO_FLAGS_HDTV_720p) != 0;

    case XB_VIDEO_1080I:
        // Intentionally disabled for this port. 1080i is fragile here and
        // offers no practical benefit for a 320x200 software-rendered game.
        return 0;

    case XB_VIDEO_AUTO:
        return 1;

    default:
        return 0;
    }
}

XbVideoMode xb_video_resolve(XbVideoMode mode)
{
    if (mode != XB_VIDEO_AUTO)
    {
        // If user's saved choice is no longer available (AV pack changed), fall back
        if (xb_video_mode_available(mode))
            return mode;
    }

    // AUTO: safe default is always 480i (640x480).
    // The user must explicitly select a higher mode via the Setup menu.
    // This prevents crashes on first boot from attempting an unsupported mode.
    return XB_VIDEO_480I;
}

void xb_video_dimensions(XbVideoMode mode, int* w, int* h)
{
    switch (mode)
    {
    case XB_VIDEO_720P:
        *w = 1280; *h = 720;
        return;
    case XB_VIDEO_1080I:
        // 1080i is intentionally not used by this port.
        *w = 640; *h = 480;
        return;
    case XB_VIDEO_480P:
    case XB_VIDEO_480I:
    default:
        *w = 640; *h = 480;
        return;
    }
}

unsigned int xb_video_present_flags(XbVideoMode mode)
{
    switch (mode)
    {
    case XB_VIDEO_720P:
        return D3DPRESENTFLAG_PROGRESSIVE | D3DPRESENTFLAG_WIDESCREEN;

    case XB_VIDEO_480P:
        return D3DPRESENTFLAG_PROGRESSIVE;

    case XB_VIDEO_480I:
    default:
        // Keep the original known-good 480i path.  The old stable build used
        // no explicit flags here.
        return 0;
    }
}

unsigned int xb_presentation_interval(XbVideoMode mode)
{
    // Use IMMEDIATE for all modes — same as the original working code.
    (void)mode;
    return D3DPRESENT_INTERVAL_IMMEDIATE;
}