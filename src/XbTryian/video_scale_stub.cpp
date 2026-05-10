// video_scale_stub.cpp — Xbox port video scale stub
//
// Replaces both video_scale.c and video_scale_hqNx.c entirely.
//
// On Xbox, software scaling is unnecessary — video.cpp performs the
// 320x200 → 640x480 upscale in hardware via GPU texture stretching.
// This file exists only to satisfy the scalers[] / scaler / scalers_count
// symbols referenced by opentyr.c's setup menu code.
//
// video_scale_hqNx.c (185KB of generated hq2x/hq3x/hq4x code) is deleted
// from the project. The hqNx entries are simply absent from scalers[].

#include "video_scale.h"

// =============================================================================
// Scaler table — one entry so the setup menu shows "1x" and works correctly.
// Width/height reported as vga_width x vga_height; actual output resolution
// (640x480) is handled by video.cpp's GPU stretch pass.
// =============================================================================

const struct Scalers scalers[] =
{
    { 320, 200, NULL, NULL, "1x" },
};

const uint scalers_count = 1;

uint scaler = 0;

// =============================================================================
// set_scaler_by_name — called by config loading; one scaler so always index 0.
// =============================================================================

void set_scaler_by_name(const char* name)
{
    (void)name;
    scaler = 0;
}