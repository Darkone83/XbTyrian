// video.cpp — Xbox D3D8 video backend for OpenTyrian
//
// Replaces video.c entirely.
//
// Architecture:
//   - Three SDL_Surface structs backed by plain heap pixel buffers (8bpp indexed,
//     320x200). The game writes directly to ->pixels without locking, just as it
//     did against SDL software surfaces.
//   - A single 320x200 linear D3DFMT_LIN_X8R8G8B8 D3DTexture is the frame buffer.
//   - JE_showVGA() expands VGAScreen's indexed pixels through rgb_palette[]
//     (pre-built in X8R8G8B8 format by palette.c), locks the linear texture,
//     copies the result, unlocks, draws a fullscreen TRIANGLESTRIP quad,
//     and calls D3DDevice_Swap(0).
//   - The GPU stretches 320x200 → 640x480 with point (nearest-neighbor) filtering,
//     giving sharp 2x pixels on a standard TV.
//
// SDL_PixelFormat:
//   A static instance with BitsPerPixel=32 is provided for palette.c's SDL_MapRGB
//   calls. Our SDL_MapRGB packs D3DFMT_X8R8G8B8 (0x00RRGGBB), so rgb_palette[] is
//   ready for direct use here without re-packing.

#include <xtl.h>
#include "video.h"
#include "video_scale.h"
#include "palette.h"

#include <stdlib.h>   // malloc, free
#include <string.h>   // memset

// =============================================================================
// Constants
// =============================================================================

#define BB_WIDTH  640
#define BB_HEIGHT 480

// Xbox D3D linear textures are CPU writable with normal row pitch.
// Swizzled/default textures are not safe for direct linear CPU copies.
#ifndef D3DFMT_LIN_X8R8G8B8
#define D3DFMT_LIN_X8R8G8B8 ((D3DFORMAT)0x0000001e)
#endif

// FVF for pre-transformed textured vertices used by the fullscreen blit quad
#define BLIT_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)

struct BlitVertex
{
    float x, y, z, rhw;
    float u, v;
};

// Fullscreen quad — static, built once.
// Covers the full 640x480 backbuffer.
//
// Important Xbox D3D detail:
// Linear textures use texel-space coordinates, not normalized 0.0f→1.0f UVs.
// If we use 0→1 here, the GPU samples only the first texel area, which makes
// the game framebuffer look black even though D3D clear/swap diagnostics work.
static const BlitVertex s_quad[4] =
{
    {        0.0f,        0.0f, 0.0f, 1.0f,          0.0f,          0.0f },  // top-left
    { BB_WIDTH + 0.f,       0.0f, 0.0f, 1.0f,  (float)vga_width,          0.0f },  // top-right
    {        0.0f, BB_HEIGHT + 0.f, 0.0f, 1.0f,          0.0f, (float)vga_height },  // bottom-left
    { BB_WIDTH + 0.f, BB_HEIGHT + 0.f, 0.0f, 1.0f,  (float)vga_width, (float)vga_height },  // bottom-right
};

// =============================================================================
// Internal state
// =============================================================================

static D3DTexture* s_pFrameTex = NULL;

static Uint8* s_pxBuf0 = NULL;   // backing store for VGAScreen
static Uint8* s_pxBuf1 = NULL;   // backing store for VGAScreen2
static Uint8* s_pxBuf2 = NULL;   // backing store for game_screen

static SDL_Surface    s_surf0;
static SDL_Surface    s_surf1;
static SDL_Surface    s_surf2;

// s_pixFmt8: surfaces are 8bpp indexed — sprite renderers check BitsPerPixel == 8
static SDL_PixelFormat s_pixFmt8 = { 8 };
// s_pixFmt32: used by palette.c SDL_MapRGB to produce D3DFMT_X8R8G8B8 values
static SDL_PixelFormat s_pixFmt32 = { 32 };

// =============================================================================
// Exported globals — declared extern in video.h
// =============================================================================

SDL_Surface* VGAScreen = NULL;
SDL_Surface* VGAScreenSeg = NULL;   // always == VGAScreen (same surface)
SDL_Surface* VGAScreen2 = NULL;
SDL_Surface* game_screen = NULL;

SDL_Window* main_window = NULL;     // unused on Xbox
SDL_PixelFormat* main_window_tex_format = &s_pixFmt32;

int             fullscreen_display = 0;        // always fullscreen on Xbox
ScalingMode     scaling_mode = SCALE_ASPECT_4_3;

const char* const scaling_mode_names[ScalingMode_MAX] =
{
    "Center",
    "Integer",
    "Fit 8:5",
    "Fit 4:3",
};

// =============================================================================
// Internal helpers
// =============================================================================

static void alloc_surface(SDL_Surface* s, Uint8** buf, int w, int h)
{
    *buf = (Uint8*)malloc(w * h);
    memset(*buf, 0, w * h);
    s->pixels = *buf;
    s->pitch = w;
    s->w = w;
    s->h = h;
    s->format = &s_pixFmt8;
}

static void setup_render_states(void)
{
    // 2D blit pass: no depth test, no lighting, no culling, no blending.
    D3DDevice_SetRenderState(D3DRS_ZENABLE, FALSE);
    D3DDevice_SetRenderState(D3DRS_LIGHTING, FALSE);
    D3DDevice_SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    D3DDevice_SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    // Texture stage 0: output texture color directly.
    D3DDevice_SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    D3DDevice_SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    D3DDevice_SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

    // Point (nearest-neighbor) filtering — sharp pixel-art upscale.
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT);
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT);
    D3DDevice_SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);

    // Clamp UVs so edge pixels don't bleed.
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    D3DDevice_SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);

    D3DDevice_SetVertexShader(BLIT_FVF);
}

// =============================================================================
// init_video / deinit_video
// =============================================================================

void init_video(void)
{
    if (s_pFrameTex)
        return;   // already initialized

    // Allocate 8bpp indexed pixel buffers and wire up SDL_Surface structs.
    alloc_surface(&s_surf0, &s_pxBuf0, vga_width, vga_height);
    alloc_surface(&s_surf1, &s_pxBuf1, vga_width, vga_height);
    alloc_surface(&s_surf2, &s_pxBuf2, vga_width, vga_height);

    VGAScreen = VGAScreenSeg = &s_surf0;
    VGAScreen2 = &s_surf1;
    game_screen = &s_surf2;

    // Create the 320x200 linear XRGB frame texture. No mipmaps.
    // Pitch is 320*4 = 1280 bytes = 20 * D3DTEXTURE_PITCH_ALIGNMENT (64): aligned.
    // This texture is filled by CPU row copies every JE_showVGA().
    s_pFrameTex = D3DDevice_CreateTexture2(
        vga_width, vga_height,
        1,               // depth (1 for 2D)
        1,               // mip levels
        0,               // usage flags
        D3DFMT_LIN_X8R8G8B8,
        D3DRTYPE_TEXTURE
    );

    setup_render_states();
}

void deinit_video(void)
{
    if (s_pFrameTex)
    {
        s_pFrameTex->Release();
        s_pFrameTex = NULL;
    }

    free(s_pxBuf0); s_pxBuf0 = NULL;
    free(s_pxBuf1); s_pxBuf1 = NULL;
    free(s_pxBuf2); s_pxBuf2 = NULL;

    VGAScreen = VGAScreenSeg = VGAScreen2 = game_screen = NULL;
}

// =============================================================================
// JE_showVGA — the hot path, called every frame
// =============================================================================

void JE_showVGA(void)
{
    // Lock the frame texture for CPU write.
    D3DLOCKED_RECT lr;
    s_pFrameTex->LockRect(0, &lr, NULL, 0);

    const Uint8* src = (const Uint8*)VGAScreen->pixels;
    Uint32* dst = (Uint32*)lr.pBits;
    int          dstStride = lr.Pitch / sizeof(Uint32);

    // Expand 8bpp indexed → D3DFMT_X8R8G8B8 using palette built by palette.c.
    // rgb_palette[] is already in 0x00RRGGBB format matching our texture format.
    for (int y = 0; y < vga_height; y++)
    {
        const Uint8* row = src + y * vga_width;
        Uint32* out = dst + y * dstStride;
        for (int x = 0; x < vga_width; x++)
            out[x] = rgb_palette[row[x]];
    }

    s_pFrameTex->UnlockRect(0);

    // Bind texture and draw the fullscreen quad.
    // GPU stretches 320x200 → 640x480 with point filter.
    D3DDevice_SetTexture(0, s_pFrameTex);
    D3DDevice_DrawVerticesUP(D3DPT_TRIANGLESTRIP, 4, s_quad, sizeof(BlitVertex));

    D3DDevice_Swap(0);
}

// =============================================================================
// JE_clr256 — clear a surface to palette index 0 (black)
// =============================================================================

void JE_clr256(SDL_Surface* screen)
{
    if (screen && screen->pixels)
        memset(screen->pixels, 0, screen->w * screen->h);
}

// =============================================================================
// Scaler API — no-ops on Xbox; we always output 640x480 via GPU stretch.
// The scalers[] table in video_scale_stub.cpp satisfies opentyr.c's menu code.
// =============================================================================

bool init_scaler(unsigned int new_scaler)
{
    scaler = (new_scaler < scalers_count) ? new_scaler : 0;
    return true;
}

bool set_scaling_mode_by_name(const char* name)
{
    (void)name;
    return false;
}

// =============================================================================
// Fullscreen / windowed management — always fullscreen on Xbox, all no-ops.
// =============================================================================

void reinit_fullscreen(int new_display) { (void)new_display; }
void video_on_win_resize(void) {}
void toggle_fullscreen(void) {}

// =============================================================================
// Coordinate mapping — no mouse on Xbox; these are no-ops.
// =============================================================================

void mapScreenPointToWindow(Sint32* inout_x, Sint32* inout_y)
{
    (void)inout_x; (void)inout_y;
}

void mapWindowPointToScreen(Sint32* inout_x, Sint32* inout_y)
{
    (void)inout_x; (void)inout_y;
}

void scaleWindowDistanceToScreen(Sint32* inout_x, Sint32* inout_y)
{
    (void)inout_x; (void)inout_y;
}