// loudness_stub.cpp — Phase 4 audio stub
// Phase 5 replaces this file with the DirectSound implementation.

#include "loudness.h"

// audioSampleRate: target rate for the sample converter in nortsong.cpp.
// Must match the rate used by the DirectSound buffer in Phase 5.
int audioSampleRate = 44100;

unsigned int song_playing = 0;
bool         audio_disabled = true;   // disabled until Phase 5 lands
bool         music_disabled = false;
bool         samples_disabled = false;

bool init_audio(void) { return false; }
void deinit_audio(void) {}

void load_music(void) {}
void play_song(unsigned int song_num) { (void)song_num; }
void restart_song(void) {}
void stop_song(void) {}
void fade_song(void) {}

void set_volume(Uint8 musicVolume, Uint8 sampleVolume)
{
    (void)musicVolume; (void)sampleVolume;
}

void multiSamplePlay(const Sint16* samples, size_t sampleCount, Uint8 chan, Uint8 vol)
{
    (void)samples; (void)sampleCount; (void)chan; (void)vol;
}