// main.cpp — Xbox entry point for OpenTyrian

#include <xtl.h>
#include "xbox_config.h"
#include "video.h"

int tyrian_main(int argc, char* argv[]);
void setupMenu(void);

// Exported so video.cpp can build the correct quad geometry
int g_bbWidth = 640;
int g_bbHeight = 480;
int g_aspectStretch = 0;   // XbAspectMode: 0=4:3, 1=stretch, 2=pixel-perfect
int g_scanlines = 0;   // 0 = off
int g_textureFilter = 0;   // 0 = sharp/point, 1 = smooth/linear

int g_brightness = 0;

void __cdecl main()
{
    // -------------------------------------------------------------------------
    // Load Xbox-specific settings (video mode, aspect, scanlines)
    // -------------------------------------------------------------------------
    XbConfig cfg;
    xb_config_load(&cfg);

    // Resolve the saved video mode at boot only.  Do not live-switch modes.
    // The OpenTyrian software renderer stays 320x200; only the Xbox backbuffer
    // changes and the GPU scales the 320x200 texture to the output mode.
    XbVideoMode resolvedMode = xb_video_resolve(cfg.videoMode);

    xb_video_dimensions(resolvedMode, &g_bbWidth, &g_bbHeight);
    g_aspectStretch = (int)cfg.aspectMode;
    g_scanlines = (int)cfg.scanlines;
    g_textureFilter = (int)cfg.textureFilter;

    // -------------------------------------------------------------------------
    // D3D device creation — 480i/480p stay 640x480; 720p uses 1280x720.
    // -------------------------------------------------------------------------
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = g_bbWidth;
    pp.BackBufferHeight = g_bbHeight;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.EnableAutoDepthStencil = FALSE;
    pp.FullScreen_PresentationInterval = xb_presentation_interval(resolvedMode);
    pp.Flags = xb_video_present_flags(resolvedMode);

    Direct3DCreate8(D3D_SDK_VERSION);

    HRESULT hr = Direct3D_CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        NULL,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &pp,
        NULL
    );

    if (FAILED(hr) && resolvedMode != XB_VIDEO_480I)
    {
        // Safe fallback: if a selected HD mode fails device creation, retry the
        // original known-good 640x480 path.
        resolvedMode = XB_VIDEO_480I;
        g_bbWidth = 640;
        g_bbHeight = 480;

        ZeroMemory(&pp, sizeof(pp));
        pp.BackBufferWidth = 640;
        pp.BackBufferHeight = 480;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferCount = 1;
        pp.MultiSampleType = D3DMULTISAMPLE_NONE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.EnableAutoDepthStencil = FALSE;
        pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        pp.Flags = 0;

        Direct3D_CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            NULL,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &pp,
            NULL
        );
    }

    // Let the AV encoder / backbuffer settle before init_video() starts
    // creating textures and drawing the first splash.
    Sleep(250);

    // -------------------------------------------------------------------------
    // Hand off to OpenTyrian
    // -------------------------------------------------------------------------
    tyrian_main(0, NULL);
}