#pragma once
// xbox_config.h — Xbox-specific video/display settings
//
// Saved to E:\TYRIAN\xbtyrian.cfg as a small binary struct.
// Loaded before D3D device creation; applied to present params and quad geometry.

#ifdef __cplusplus
extern "C" {
#endif

    // Video output resolution
    typedef enum XbVideoMode
    {
        XB_VIDEO_AUTO = 0,   // pick best available
        XB_VIDEO_480P = 1,   // 640x480 progressive
        XB_VIDEO_720P = 2,   // 1280x720
        XB_VIDEO_1080I = 3,   // 1920x1080 interlaced
        XB_VIDEO_480I = 4,   // 640x480 interlaced (SD fallback)
    } XbVideoMode;

    // Aspect ratio / scaling behaviour
    typedef enum XbAspectMode
    {
        XB_ASPECT_4_3 = 0,              // 4:3 corrected / pillarbox
        XB_ASPECT_STRETCH = 1,          // stretch to fill screen
        XB_ASPECT_PIXEL_PERFECT = 2,    // integer scale, centered / contain
        XB_ASPECT_PIXEL_FILL = 3        // integer scale, fill screen / crop
    } XbAspectMode;

    // Scanline overlay
    typedef enum XbScanlines
    {
        XB_SCANLINES_OFF = 0,
        XB_SCANLINES_LIGHT = 1,
        XB_SCANLINES_MEDIUM = 2,
        XB_SCANLINES_HEAVY = 3,
    } XbScanlines;

    // Final brightness preset for the 320x200 -> backbuffer upscale
    typedef enum XbBrightness
    {
        XB_BRIGHTNESS_NORMAL = 0,
        XB_BRIGHTNESS_BRIGHT = 1,
    } XbBrightness;

    // Final texture filtering for the 320x200 -> backbuffer upscale
    typedef enum XbTextureFilter
    {
        XB_TEXTURE_FILTER_SHARP = 0,  // D3DTEXF_POINT / nearest-neighbor
        XB_TEXTURE_FILTER_SMOOTH = 1, // D3DTEXF_LINEAR
    } XbTextureFilter;

    typedef struct XbConfig
    {
        XbVideoMode      videoMode;
        XbAspectMode     aspectMode;
        XbScanlines      scanlines;
        XbTextureFilter  textureFilter;
        XbBrightness     brightness;
    } XbConfig;

    // Default settings
#define XB_CONFIG_DEFAULT { XB_VIDEO_AUTO, XB_ASPECT_4_3, XB_SCANLINES_OFF, XB_TEXTURE_FILTER_SHARP, XB_BRIGHTNESS_NORMAL }

// Load settings from E:\TYRIAN\xbtyrian.cfg — fills *cfg with defaults if missing
    void xb_config_load(XbConfig* cfg);

    // Save settings to E:\TYRIAN\xbtyrian.cfg
    void xb_config_save(const XbConfig* cfg);

    // Query what modes the connected AV pack actually supports
    // Returns non-zero if the mode is available on this hardware
    int xb_video_mode_available(XbVideoMode mode);

    // Resolve AUTO to a concrete mode based on available flags
    XbVideoMode xb_video_resolve(XbVideoMode mode);

    // Get backbuffer dimensions for a resolved mode
    void xb_video_dimensions(XbVideoMode mode, int* w, int* h);

    // Xbox D3D presentation flags for a resolved mode.
    // 720p needs both PROGRESSIVE and WIDESCREEN.
    unsigned int xb_video_present_flags(XbVideoMode mode);

    // Presentation interval flag for a resolved mode
    unsigned int xb_presentation_interval(XbVideoMode mode);

#ifdef __cplusplus
}
#endif