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
#include "xbox_config.h"
#include "palette.h"

#include <stdlib.h>   // malloc, free
#include <string.h>   // memset

// =============================================================================
// Constants
// =============================================================================

// BB_WIDTH / BB_HEIGHT come from main.cpp via g_bbWidth / g_bbHeight
// and are read at init_video() time to build the correct quad.

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

// Fullscreen quad — built dynamically in init_video() based on resolution and
// aspect mode. Xbox D3D linear textures use texel-space UVs.
static BlitVertex s_quad[4];

// Exported from main.cpp — set before init_video() is called
extern int g_bbWidth;
extern int g_bbHeight;
extern int g_aspectStretch;   // XbAspectMode: 0=4:3, 1=stretch, 2=pixel-perfect
extern int g_scanlines;       // 0=off, 1=light, 2=heavy
extern int g_textureFilter;   // 0=point/sharp, 1=linear/smooth
extern int g_brightness;      // 0=normal, 1=bright

// Build quad vertices for the given backbuffer size and scale/aspect mode.
// 4:3 mode: centers a corrected 4:3 quad.
// 16:9 stretch: fills the screen.
// Pixel Perfect: integer-scales 320x200 and centers the result.
static void build_quad(int bbW, int bbH, int aspectMode)
{
    float qx0, qy0, qx1, qy1;

    if (aspectMode == XB_ASPECT_STRETCH)
    {
        // Fill entire backbuffer
        qx0 = 0.0f;       qy0 = 0.0f;
        qx1 = (float)bbW; qy1 = (float)bbH;
    }
    else if (aspectMode == XB_ASPECT_PIXEL_PERFECT ||
        aspectMode == XB_ASPECT_PIXEL_FILL)
    {
        // Integer scale from the original 320x200 framebuffer.
        //
        // PIXEL PERFECT = contain:
        //   720p: 320x200 -> 960x600 centered inside 1280x720.
        //
        // PIXEL FILL = cover:
        //   720p: 320x200 -> 1280x800 centered, cropping 40px top/bottom.
        int scaleX = bbW / vga_width;
        int scaleY = bbH / vga_height;

        int scale;
        if (aspectMode == XB_ASPECT_PIXEL_FILL)
        {
            scale = (scaleX > scaleY) ? scaleX : scaleY;
            if ((vga_width * scale) < bbW || (vga_height * scale) < bbH)
                ++scale;
        }
        else
        {
            scale = (scaleX < scaleY) ? scaleX : scaleY;
        }

        if (scale < 1)
            scale = 1;

        float targetW = (float)(vga_width * scale);
        float targetH = (float)(vga_height * scale);

        qx0 = ((float)bbW - targetW) * 0.5f;
        qy0 = ((float)bbH - targetH) * 0.5f;
        qx1 = qx0 + targetW;
        qy1 = qy0 + targetH;
    }
    else
    {
        // 4:3 pillarbox/corrected mode: game is 320x200 with non-square pixels.
        // Target a 4:3 rect centered in the backbuffer.
        float targetH = (float)bbH;
        float targetW = targetH * 4.0f / 3.0f;

        if (targetW > (float)bbW)
        {
            targetW = (float)bbW;
            targetH = targetW * 3.0f / 4.0f;
        }

        qx0 = ((float)bbW - targetW) * 0.5f;
        qy0 = ((float)bbH - targetH) * 0.5f;
        qx1 = qx0 + targetW;
        qy1 = qy0 + targetH;
    }

    float tw = (float)vga_width;
    float th = (float)vga_height;

    s_quad[0].x = qx0; s_quad[0].y = qy0; s_quad[0].z = 0.0f; s_quad[0].rhw = 1.0f; s_quad[0].u = 0.0f; s_quad[0].v = 0.0f;
    s_quad[1].x = qx1; s_quad[1].y = qy0; s_quad[1].z = 0.0f; s_quad[1].rhw = 1.0f; s_quad[1].u = tw;   s_quad[1].v = 0.0f;
    s_quad[2].x = qx0; s_quad[2].y = qy1; s_quad[2].z = 0.0f; s_quad[2].rhw = 1.0f; s_quad[2].u = 0.0f; s_quad[2].v = th;
    s_quad[3].x = qx1; s_quad[3].y = qy1; s_quad[3].z = 0.0f; s_quad[3].rhw = 1.0f; s_quad[3].u = tw;   s_quad[3].v = th;
}

// =============================================================================
// Internal state
// =============================================================================

static D3DTexture* s_pFrameTex = NULL;
static int          s_scanlines = 0;       // 0=off 1=light 2=medium 3=heavy
static int          s_textureFilter = 0;   // 0=point/sharp 1=linear/smooth
static int          s_brightness = 0;      // 0=normal 1=bright

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

    // Final texture filtering for the 320x200 -> backbuffer upscale.
    // Sharp = nearest-neighbor pixel look, Smooth = linear filtering.
    const DWORD filter = (s_textureFilter != 0) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    D3DDevice_SetTextureStageState(0, D3DTSS_MINFILTER, filter);
    D3DDevice_SetTextureStageState(0, D3DTSS_MAGFILTER, filter);
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

    // Build the output quad based on resolution and aspect mode
    // g_bbWidth/g_bbHeight/g_aspectStretch/g_scanlines set by main.cpp before init_video
    build_quad(g_bbWidth, g_bbHeight, g_aspectStretch);
    s_scanlines = g_scanlines;
    s_textureFilter = g_textureFilter;
    s_brightness = g_brightness;

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
// video_apply_settings — apply aspect/scanline changes without reinit
// Call this after changing settings in the setup menu.
// =============================================================================

void video_apply_settings(int aspectMode, int scanlines, int textureFilter, int brightness)
{
    g_aspectStretch = aspectMode;
    g_scanlines = scanlines;
    g_textureFilter = textureFilter;
    g_brightness = brightness;
    s_scanlines = scanlines;
    s_textureFilter = textureFilter;
    s_brightness = brightness;
    build_quad(g_bbWidth, g_bbHeight, aspectMode);
    setup_render_states();
}

// =============================================================================
// JE_showVGA — the hot path, called every frame
// =============================================================================

void JE_showVGA(void)
{
    if (s_pFrameTex == NULL || VGAScreen == NULL || VGAScreen->pixels == NULL)
        return;

    // Lock the frame texture for CPU write.
    D3DLOCKED_RECT lr;
    if (FAILED(s_pFrameTex->LockRect(0, &lr, NULL, 0)))
        return;

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
        {
            Uint32 c = rgb_palette[row[x]];

            if (s_brightness == 1)
            {
                int r = (int)((c >> 16) & 0xff);
                int g = (int)((c >> 8) & 0xff);
                int b = (int)(c & 0xff);

                // Mild display-side brightness boost.  Keep it conservative
                // so Tyrian's palette does not wash out.
                r = r + (r >> 3) + 6;
                g = g + (g >> 3) + 6;
                b = b + (b >> 3) + 6;

                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;

                c = (Uint32)((r << 16) | (g << 8) | b);
            }

            out[x] = c;
        }
    }

    s_pFrameTex->UnlockRect(0);

    // Own the full render pass explicitly.  This is more reliable for 720p
    // than relying on raw draw calls outside a scene.
    D3DDevice_BeginScene();

    // Clear the whole backbuffer first so 4:3 pillarbox areas are always black.
    D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

    // Re-apply state defensively.  Resolution changes/device setup can leave
    // stale state behind, and this path is cheap compared to the CPU upload.
    setup_render_states();

    // Bind texture and draw the scaled quad.
    // GPU stretches 320x200 → selected backbuffer with point filtering.
    D3DDevice_SetTexture(0, s_pFrameTex);
    D3DDevice_DrawVerticesUP(D3DPT_TRIANGLESTRIP, 4, s_quad, sizeof(BlitVertex));

    // Scanline overlay — darken alternate rows using solid black rects.
    if (s_scanlines != 0)
    {
        int scanH = g_bbHeight / 200;
        if (scanH < 1) scanH = 1;

        int gapH = 1;
        if (s_scanlines == 2)       // Medium
            gapH = (scanH > 1) ? ((scanH + 1) / 2) : 1;
        else if (s_scanlines >= 3)  // Heavy
            gapH = scanH;

        for (int row = scanH; row < g_bbHeight; row += scanH * 2)
        {
            D3DRECT r;
            r.x1 = 0; r.x2 = g_bbWidth;
            r.y1 = row;
            r.y2 = row + gapH;
            if (r.y2 > g_bbHeight)
                r.y2 = g_bbHeight;
            D3DDevice_Clear(1, &r, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);
        }
    }

    D3DDevice_EndScene();
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