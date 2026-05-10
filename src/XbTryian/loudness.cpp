// loudness.cpp — Xbox DirectSound audio backend for OpenTyrian
//
// Replaces loudness.c entirely.
//
// Architecture:
//   SDL audio device + callback  →  DirectSound streaming ring buffer + audio thread
//   SDL_LockAudioDevice          →  EnterCriticalSection
//   SDL_UnlockAudioDevice        →  LeaveCriticalSection
//
// The audioCallback body is preserved exactly from the original — it mixes
// 8 PCM channels with OPL FM output into a Sint16 mono stream. The only
// change is that it's called from our AudioThread rather than SDL's driver.
//
// Ring buffer: RING_BUFFER_COUNT × CALLBACK_BYTES of looping DirectSound
// buffer. AudioThread wakes every CALLBACK_MS ms, locks the next chunk,
// calls audioCallback to fill it, unlocks, advances the write pointer.

#include <xtl.h>
#include <dsound.h>
#include <math.h>

#include "loudness.h"

#include "file.h"
#include "lds_play.h"
#include "nortsong.h"
#include "opentyr.h"
#include "params.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// Constants
// =============================================================================

#define OUTPUT_QUALITY       4                              // 44.1 kHz target
#define CALLBACK_SAMPLES     (256 * OUTPUT_QUALITY)        // 1024 samples ≈ 23ms
#define CALLBACK_BYTES       (CALLBACK_SAMPLES * (int)sizeof(Sint16))
#define RING_BUFFER_COUNT    4                              // 4 callbacks of headroom
#define RING_BUFFER_BYTES    (CALLBACK_BYTES * RING_BUFFER_COUNT)
#define CALLBACK_MS          23                             // thread sleep period

// =============================================================================
// Exported state — declared extern in loudness.h
// =============================================================================

int          audioSampleRate = 0;
unsigned int song_playing = 0;
bool         audio_disabled = false;
bool         music_disabled = false;
bool         samples_disabled = false;

// =============================================================================
// Internal state
// =============================================================================

static bool music_stopped = true;

static LPDIRECTSOUND       s_pDS = NULL;
static LPDIRECTSOUNDBUFFER s_pBuf = NULL;
static HANDLE              s_hThread = NULL;
static HANDLE              s_hStop = NULL;
static CRITICAL_SECTION    s_cs;
static bool                s_cs_initialized = false;
static DWORD               s_writePos = 0;
static DWORD               s_writeChunk = 0;

static Uint8  musicVolume = 255;
static Uint8  sampleVolume = 255;

static const float volumeRange = 30.0f;  // dB

// Inline float→int to avoid __ftol2_sse in TO_FIXED macro
static __inline Sint32 loudness_ftoi(float x)
{
    Sint32 result;
    __asm fld x
    __asm fistp result
    return result;
}

// Fixed-point Q20.12 volume table
static Sint32 volumeFactorTable[256];
#define TO_FIXED(x)     loudness_ftoi((x) * (1 << 12))
#define FIXED_TO_INT(x) ((Sint32)((x) >> 12))

// LDS timing — same constants and logic as original
static const int ldsUpdate2Rate = 139;  // 69.5 Hz × 2
static int samplesPerLdsUpdate;
static int samplesPerLdsUpdateFrac;
static int samplesUntilLdsUpdate = 0;
static int samplesUntilLdsUpdateFrac = 0;

// Music file
static FILE* music_file = NULL;
static Uint32* song_offset = NULL;
static Uint16  song_count = 0;

// PCM mixer channels
#define CHANNEL_COUNT        8
#define CHANNEL_VOLUME_LEVELS 8
static const Sint16* channelSamples[CHANNEL_COUNT];
static size_t        channelSampleCount[CHANNEL_COUNT] = { 0 };
static Uint8         channelVolume[CHANNEL_COUNT];

// =============================================================================
// Forward declarations
// =============================================================================

static void  audioCallback(void* userdata, Uint8* stream, int size);
static void  load_song(unsigned int song_num);
static DWORD WINAPI AudioThread(LPVOID param);

// =============================================================================
// audioCallback — preserved verbatim from loudness.c
//
// Called from AudioThread while s_cs is held, so all shared state
// (channelSamples, music_stopped, volume, etc.) is safely serialised.
// =============================================================================

static void audioCallback(void* userdata, Uint8* stream, int size)
{
    (void)userdata;

    Sint16* const samples = (Sint16*)stream;
    const int     samplesCount = size / sizeof(Sint16);

    if (!music_disabled && !music_stopped)
    {
        Sint16* remaining = samples;
        int     remainingCount = samplesCount;
        while (remainingCount > 0)
        {
            if (samplesUntilLdsUpdate == 0)
            {
                lds_update();

                samplesUntilLdsUpdate += samplesPerLdsUpdate;
                samplesUntilLdsUpdateFrac += samplesPerLdsUpdateFrac;
                if (samplesUntilLdsUpdateFrac >= ldsUpdate2Rate)
                {
                    samplesUntilLdsUpdate += 1;
                    samplesUntilLdsUpdateFrac -= ldsUpdate2Rate;
                }
            }

            int count = MIN(samplesUntilLdsUpdate, remainingCount);

            opl_update(remaining, count);

            remaining += count;
            remainingCount -= count;

            samplesUntilLdsUpdate -= count;
        }
    }
    else
    {
        for (int i = 0; i < samplesCount; ++i)
            samples[i] = 0;
    }

    Sint32 musicVolumeFactor = volumeFactorTable[musicVolume];
    musicVolumeFactor *= 2;  // OPL emulator is too quiet

    if (samples_disabled && !music_disabled)
    {
        Sint16* remaining = samples;
        int     remainingCount = samplesCount;
        while (remainingCount > 0)
        {
            Sint32 sample = *remaining * musicVolumeFactor;
            sample = FIXED_TO_INT(sample);
            *remaining = (Sint16)MIN(MAX(INT16_MIN, sample), INT16_MAX);
            remaining += 1;
            remainingCount -= 1;
        }
    }
    else if (!samples_disabled)
    {
        Sint32 sampleVolumeFactor = volumeFactorTable[sampleVolume];
        Sint32 sampleVolumeFactors[CHANNEL_VOLUME_LEVELS];
        for (int i = 0; i < CHANNEL_VOLUME_LEVELS; ++i)
            sampleVolumeFactors[i] = sampleVolumeFactor * (i + 1) / CHANNEL_VOLUME_LEVELS;

        Sint16* remaining = samples;
        int     remainingCount = samplesCount;
        while (remainingCount > 0)
        {
            Sint32 sample = *remaining * musicVolumeFactor;

            for (size_t i = 0; i < CHANNEL_COUNT; ++i)
            {
                if (channelSampleCount[i] > 0)
                {
                    sample += *channelSamples[i] * sampleVolumeFactors[channelVolume[i]];
                    channelSamples[i] += 1;
                    channelSampleCount[i] -= 1;
                }
            }

            sample = FIXED_TO_INT(sample);
            *remaining = (Sint16)MIN(MAX(INT16_MIN, sample), INT16_MAX);
            remaining += 1;
            remainingCount -= 1;
        }
    }
}

// =============================================================================
// AudioThread — feeds the DirectSound ring buffer every CALLBACK_MS ms
// =============================================================================

static DWORD WINAPI AudioThread(LPVOID param)
{
    (void)param;

    while (WaitForSingleObject(s_hStop, 2) == WAIT_TIMEOUT)
    {
        DWORD playCursor = 0;
        DWORD writeCursor = 0;

        if (FAILED(s_pBuf->GetCurrentPosition(&playCursor, &writeCursor)))
            continue;

        const DWORD playChunk = playCursor / (DWORD)CALLBACK_BYTES;

        // Refill only chunks the hardware has already moved past.
        // This keeps music generation locked to the DirectSound play cursor
        // instead of to Sleep() timing, and avoids overwriting the active chunk.
        while (s_writeChunk != playChunk)
        {
            void* ptr1 = NULL;
            void* ptr2 = NULL;
            DWORD bytes1 = 0;
            DWORD bytes2 = 0;

            s_writePos = s_writeChunk * (DWORD)CALLBACK_BYTES;

            HRESULT hr = s_pBuf->Lock(s_writePos, (DWORD)CALLBACK_BYTES,
                &ptr1, &bytes1, &ptr2, &bytes2, 0);
            if (FAILED(hr))
                break;

            // Hold the critical section for the duration of the callback so
            // main-thread lock/unlock calls block cleanly around state changes.
            EnterCriticalSection(&s_cs);

            audioCallback(NULL, (Uint8*)ptr1, (int)bytes1);
            if (ptr2 && bytes2)
                audioCallback(NULL, (Uint8*)ptr2, (int)bytes2);

            LeaveCriticalSection(&s_cs);

            s_pBuf->Unlock(ptr1, bytes1, ptr2, bytes2);

            s_writeChunk = (s_writeChunk + 1) % (DWORD)RING_BUFFER_COUNT;
        }
    }

    return 0;
}

// =============================================================================
// init_audio
// =============================================================================

bool init_audio(void)
{
    if (audio_disabled)
        return false;

    InitializeCriticalSection(&s_cs);
    s_cs_initialized = true;

    // Fixed sample rate — matches the S8→S16 converter in nortsong.cpp
    audioSampleRate = 11025 * OUTPUT_QUALITY;   // 44100 Hz

    samplesPerLdsUpdate = 2 * (audioSampleRate / ldsUpdate2Rate);
    samplesPerLdsUpdateFrac = 2 * (audioSampleRate % ldsUpdate2Rate);

    // Build logarithmic volume factor table
    volumeFactorTable[0] = 0;
    for (size_t i = 1; i < 256; ++i)
        volumeFactorTable[i] = TO_FIXED(powf(10.0f, (255 - (int)i) * (-volumeRange / (20.0f * 255))));

    opl_init();

    // ---- DirectSound device -------------------------------------------------

    if (FAILED(DirectSoundCreate(NULL, &s_pDS, NULL)))
    {
        OutputDebugStringA("error: DirectSoundCreate failed\n");
        audio_disabled = true;
        if (s_cs_initialized) { DeleteCriticalSection(&s_cs); s_cs_initialized = false; }
        return false;
    }

    // ---- Streaming buffer ---------------------------------------------------

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = (DWORD)audioSampleRate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    DSBUFFERDESC dsbd;
    memset(&dsbd, 0, sizeof(dsbd));
    dsbd.dwSize = sizeof(dsbd);
    dsbd.dwFlags = 0;  // DSBCAPS_LOCSOFTWARE is PC-only; Xbox uses 0
    dsbd.dwBufferBytes = (DWORD)RING_BUFFER_BYTES;
    dsbd.lpwfxFormat = &wfx;

    if (FAILED(s_pDS->CreateSoundBuffer(&dsbd, &s_pBuf, NULL)))
    {
        OutputDebugStringA("error: CreateSoundBuffer failed\n");
        s_pDS->Release();
        s_pDS = NULL;
        audio_disabled = true;
        if (s_cs_initialized) { DeleteCriticalSection(&s_cs); s_cs_initialized = false; }
        return false;
    }

    // Pre-fill the whole ring with generated audio/silence before playback.
    // Starting with a silent ring and waiting one full callback before the
    // first write can leave the stream chasing the play cursor and cause
    // crunchy/choppy FM music on Xbox.
    void* ptr1, * ptr2;
    DWORD  bytes1, bytes2;
    if (SUCCEEDED(s_pBuf->Lock(0, (DWORD)RING_BUFFER_BYTES,
        &ptr1, &bytes1, &ptr2, &bytes2, 0)))
    {
        EnterCriticalSection(&s_cs);
        audioCallback(NULL, (Uint8*)ptr1, (int)bytes1);
        if (ptr2 && bytes2)
            audioCallback(NULL, (Uint8*)ptr2, (int)bytes2);
        LeaveCriticalSection(&s_cs);

        s_pBuf->Unlock(ptr1, bytes1, ptr2, bytes2);
    }
    s_writePos = 0;
    s_writeChunk = 0;

    // Start looping playback
    s_pBuf->Play(0, 0, DSBPLAY_LOOPING);

    // Launch the audio thread at high priority to avoid buffer underruns
    s_hStop = CreateEvent(NULL, TRUE, FALSE, NULL);
    s_hThread = CreateThread(NULL, 0, AudioThread, NULL, 0, NULL);
    if (s_hThread)
        SetThreadPriority(s_hThread, THREAD_PRIORITY_HIGHEST);

    return true;
}

// =============================================================================
// deinit_audio
// =============================================================================

void deinit_audio(void)
{
    if (audio_disabled)
        return;

    // Signal thread and wait for clean exit
    if (s_hStop)
    {
        SetEvent(s_hStop);
        if (s_hThread)
        {
            WaitForSingleObject(s_hThread, 2000);
            CloseHandle(s_hThread);
            s_hThread = NULL;
        }
        CloseHandle(s_hStop);
        s_hStop = NULL;
    }

    if (s_pBuf)
    {
        s_pBuf->Stop();
        s_pBuf->Release();
        s_pBuf = NULL;
    }

    if (s_pDS)
    {
        s_pDS->Release();
        s_pDS = NULL;
    }

    if (s_cs_initialized) { DeleteCriticalSection(&s_cs); s_cs_initialized = false; }

    memset(channelSampleCount, 0, sizeof channelSampleCount);

    lds_free();
}

// =============================================================================
// load_music / load_song
// =============================================================================

void load_music(void)
{
    if (music_file == NULL)
    {
        music_file = dir_fopen_die(data_dir(), "music.mus", "rb");

        fread_u16_die(&song_count, 1, music_file);

        song_offset = (Uint32*)malloc((song_count + 1) * sizeof(*song_offset));

        fread_u32_die(song_offset, song_count, music_file);

        song_offset[song_count] = (Uint32)ftell_eof(music_file);
    }
}

static void load_song(unsigned int song_num)
{
    if (song_num < song_count)
    {
        unsigned int song_size = song_offset[song_num + 1] - song_offset[song_num];
        lds_load(music_file, song_offset[song_num], song_size);
    }
    else
    {
        OutputDebugStringA("warning: failed to load song\n");
    }
}

// =============================================================================
// Music control — each function wraps state changes in the critical section
// to serialise with AudioThread's callback invocations.
// =============================================================================

void play_song(unsigned int song_num)
{
    if (audio_disabled)
        return;

    if (song_num != song_playing)
    {
        EnterCriticalSection(&s_cs);
        music_stopped = true;
        LeaveCriticalSection(&s_cs);

        load_song(song_num);
        song_playing = song_num;
    }

    EnterCriticalSection(&s_cs);
    music_stopped = false;
    LeaveCriticalSection(&s_cs);
}

void restart_song(void)
{
    if (audio_disabled)
        return;

    EnterCriticalSection(&s_cs);
    lds_rewind();
    music_stopped = false;
    LeaveCriticalSection(&s_cs);
}

void stop_song(void)
{
    if (audio_disabled)
        return;

    EnterCriticalSection(&s_cs);
    music_stopped = true;
    LeaveCriticalSection(&s_cs);
}

void fade_song(void)
{
    if (audio_disabled)
        return;

    EnterCriticalSection(&s_cs);
    lds_fade(1);
    LeaveCriticalSection(&s_cs);
}

void set_volume(Uint8 musicVolume_, Uint8 sampleVolume_)
{
    if (audio_disabled)
        return;

    if (!s_cs_initialized)
    {
        // Called before init_audio — just store values, no lock needed yet
        musicVolume = musicVolume_;
        sampleVolume = sampleVolume_;
        return;
    }

    EnterCriticalSection(&s_cs);
    musicVolume = musicVolume_;
    sampleVolume = sampleVolume_;
    LeaveCriticalSection(&s_cs);
}

// =============================================================================
// multiSamplePlay — queues a PCM sound effect on a mixer channel
// =============================================================================

void multiSamplePlay(const Sint16* samples, size_t sampleCount, Uint8 chan, Uint8 vol)
{
    if (!(chan < CHANNEL_COUNT)) for (;;) Sleep(1000);
    if (!(vol < CHANNEL_VOLUME_LEVELS)) for (;;) Sleep(1000);

    if (audio_disabled || samples_disabled)
        return;

    EnterCriticalSection(&s_cs);
    channelSamples[chan] = samples;
    channelSampleCount[chan] = sampleCount;
    channelVolume[chan] = vol;
    LeaveCriticalSection(&s_cs);
}