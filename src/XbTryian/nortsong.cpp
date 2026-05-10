// nortsong.cpp — Xbox port of nortsong.c
//
// Changes from original:
//   SDL_GetTicks()  → GetTickCount()
//   SDL_Delay()     → Sleep()
//   SDL_AudioCVT block in loadSndFile → manual S8@11025→S16@audioSampleRate conversion
//   #include <xtl.h> added; SDL.h no longer needed for any actual calls

#include <xtl.h>

#include "nortsong.h"

#include "file.h"
#include "joystick.h"
#include "keyboard.h"
#include "loudness.h"
#include "musmast.h"
#include "opentyr.h"
#include "params.h"
#include "sndmast.h"
#include "vga256d.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// =============================================================================
// Exported state — declared extern in nortsong.h
// =============================================================================

JE_word frameCountMax;

Sint16* soundSamples[SOUND_COUNT] = { NULL };
size_t  soundSampleCount[SOUND_COUNT] = { 0 };

JE_word tyrMusicVolume, fxVolume;
const JE_word fxPlayVol = 4;
JE_word tempVolume;

// =============================================================================
// Internal timing state
// =============================================================================

// The period of the original x86 PIT in milliseconds — unchanged from original.
static const float pitPeriod = (12.0f / 14318180.0f) * 1000.0f;

static Uint16 delaySpeed = 0x4300;
static float  delayPeriod = 0x4300 * ((12.0f / 14318180.0f) * 1000.0f);

static Uint32 target = 0;
static Uint32 target2 = 0;

// =============================================================================
// Timing functions — SDL_GetTicks → GetTickCount, SDL_Delay → Sleep
// =============================================================================

void setDelay(int delay)
{
    target = GetTickCount() + (Uint32)(delay * delayPeriod);
}

void setDelay2(int delay)
{
    target2 = GetTickCount() + (Uint32)(delay * delayPeriod);
}

Uint32 getDelayTicks(void)
{
    Sint32 delay = (Sint32)(target - GetTickCount());
    return (Uint32)MAX(0, delay);
}

Uint32 getDelayTicks2(void)
{
    Sint32 delay = (Sint32)(target2 - GetTickCount());
    return (Uint32)MAX(0, delay);
}

void wait_delay(void)
{
    Sint32 delay = (Sint32)(target - GetTickCount());
    if (delay > 0)
        Sleep((DWORD)delay);
}

void service_wait_delay(void)
{
    for (;;)
    {
        service_SDL_events(false);

        Sint32 delay = (Sint32)(target - GetTickCount());
        if (delay <= 0)
            return;

        Sleep((DWORD)MIN(delay, SDL_POLL_INTERVAL));
    }
}

void wait_delayorinput(void)
{
    for (;;)
    {
        service_SDL_events(false);
        poll_joysticks();

        if (newkey || mousedown || joydown)
        {
            newkey = false;
            return;
        }

        Sint32 delay = (Sint32)(target - GetTickCount());
        if (delay <= 0)
            return;

        Sleep((DWORD)MIN(delay, SDL_POLL_INTERVAL));
    }
}

void setDelaySpeed(Uint16 speed)
{
    delaySpeed = speed;
    delayPeriod = speed * pitPeriod;
}

// =============================================================================
// Audio sample conversion
//
// Replaces SDL_AudioCVT / SDL_BuildAudioCVT / SDL_ConvertAudio.
//
// Converts raw signed 8-bit samples at 11025 Hz to signed 16-bit samples at
// dstRate (audioSampleRate). Uses nearest-neighbor resampling — good enough for
// game sound effects, and consistent with what SDL's converter produced.
//
// Ownership: frees *pp and replaces it with a newly allocated S16 buffer.
//            *pCount is updated from byte count (S8) to sample count (S16).
// =============================================================================

static void convert_s8_to_s16(Sint16** pp, size_t* pCount, int dstRate)
{
    const Sint8* raw = (const Sint8*)(*pp);
    size_t       rawCount = *pCount;    // number of S8 bytes == number of raw samples

    if (rawCount == 0)
        return;

    // Output sample count scaled for the target rate
    size_t outCount = (size_t)((double)rawCount * dstRate / 11025.0 + 0.5);
    if (outCount == 0) outCount = 1;

    Sint16* out = (Sint16*)malloc(outCount * sizeof(Sint16));
    if (!out)
    {
        *pCount = 0;
        return;
    }

    for (size_t i = 0; i < outCount; i++)
    {
        // Nearest-neighbor: map output position back to input
        size_t srcIdx = (size_t)((double)i * 11025.0 / dstRate);
        if (srcIdx >= rawCount)
            srcIdx = rawCount - 1;

        // Scale S8 (-128..127) → S16 (-32768..32767) by factor of 256
        out[i] = (Sint16)((Sint32)raw[srcIdx] * 256);
    }

    free(*pp);
    *pp = out;
    *pCount = outCount;
}

// =============================================================================
// loadSndFile — loads tyrian.snd and voices.snd, converts samples for playback
// =============================================================================

void loadSndFile(bool xmas)
{
    FILE* f;

    // ---- SFX ----------------------------------------------------------------
    f = dir_fopen_die(data_dir(), "tyrian.snd", "rb");

    Uint16 sfxCount;
    Uint32 sfxPositions[SFX_COUNT + 1];

    fread_u16_die(&sfxCount, 1, f);
    if (sfxCount != SFX_COUNT)
        goto die;

    fread_u32_die(sfxPositions, sfxCount, f);

    fseek(f, 0, SEEK_END);
    sfxPositions[sfxCount] = ftell(f);

    for (size_t i = 0; i < sfxCount; ++i)
    {
        soundSampleCount[i] = sfxPositions[i + 1] - sfxPositions[i];

        if (soundSampleCount[i] > UINT16_MAX)
            goto die;

        free(soundSamples[i]);
        soundSamples[i] = (Sint16*)malloc(soundSampleCount[i]);

        fseek(f, sfxPositions[i], SEEK_SET);
        fread_u8_die((Uint8*)soundSamples[i], soundSampleCount[i], f);
    }

    fclose(f);

    // ---- Voices -------------------------------------------------------------
    f = dir_fopen_die(data_dir(), xmas ? "voicesc.snd" : "voices.snd", "rb");

    Uint16 voiceCount;
    Uint32 voicePositions[VOICE_COUNT + 1];

    fread_u16_die(&voiceCount, 1, f);
    if (voiceCount != VOICE_COUNT)
        goto die;

    fread_u32_die(voicePositions, voiceCount, f);

    fseek(f, 0, SEEK_END);
    voicePositions[voiceCount] = ftell(f);

    for (size_t vi = 0; vi < voiceCount; ++vi)
    {
        size_t i = SFX_COUNT + vi;

        soundSampleCount[i] = voicePositions[vi + 1] - voicePositions[vi];

        // Voice sounds have some bad data at the end
        soundSampleCount[i] = soundSampleCount[i] >= 100
            ? soundSampleCount[i] - 100
            : 0;

        if (soundSampleCount[i] > UINT16_MAX)
            goto die;

        free(soundSamples[i]);
        soundSamples[i] = (Sint16*)malloc(soundSampleCount[i]);

        fseek(f, voicePositions[vi], SEEK_SET);
        fread_u8_die((Uint8*)soundSamples[i], soundSampleCount[i], f);
    }

    fclose(f);

    // ---- Convert S8@11025Hz → S16@audioSampleRate ---------------------------
    // Replaces the SDL_BuildAudioCVT / SDL_ConvertAudio block from the original.
    // Each sample is converted individually; same end result.
    for (size_t i = 0; i < SOUND_COUNT; i++)
    {
        if (soundSamples[i] && soundSampleCount[i] > 0)
            convert_s8_to_s16(&soundSamples[i], &soundSampleCount[i], audioSampleRate);
    }

    return;

die:
    OutputDebugStringA("error: Unexpected data was read from a file.\n");
    for (;;) Sleep(1000);
}

// =============================================================================
// Remaining functions — unchanged from original
// =============================================================================

void JE_playSampleNum(JE_byte samplenum)
{
    multiSamplePlay(soundSamples[samplenum - 1], soundSampleCount[samplenum - 1],
        0, fxPlayVol);
}

void JE_changeVolume(JE_word* music, int music_delta, JE_word* sample, int sample_delta)
{
    int music_temp = *music + music_delta;
    int sample_temp = *sample + sample_delta;

    if (music_delta)
    {
        if (music_temp > 255)
        {
            music_temp = 255;
            JE_playSampleNum(S_CLINK);
        }
        else if (music_temp < 0)
        {
            music_temp = 0;
            JE_playSampleNum(S_CLINK);
        }
    }

    if (sample_delta)
    {
        if (sample_temp > 255)
        {
            sample_temp = 255;
            JE_playSampleNum(S_CLINK);
        }
        else if (sample_temp < 0)
        {
            sample_temp = 0;
            JE_playSampleNum(S_CLINK);
        }
    }

    *music = music_temp;
    *sample = sample_temp;

    set_volume((Uint8)*music, (Uint8)*sample);
}