// Melody playback module for CC3200
// sound_Effects.h

#ifndef SOUND_EFFECTS_H
#define SOUND_EFFECTS_H

#include <stdio.h>
#include <stdbool.h>

#define TIMER_CLOCK_HZ          80000000  // 80MHz clock from your original code

// Sound effect state machine
typedef struct {
    const unsigned long *tones;
    const unsigned long *durations;
    int melodyLength;
    int currentNoteIndex;
    bool playing;
    bool looping;
    unsigned long noteStartTime;
} SoundEffectState;

// Global state for sound effect playback
static SoundEffectState g_SoundState = {false, 0, 0, NULL, NULL, 0};

// Define some example melodies
// Success sound (ascending)
extern const unsigned long SUCCESS_TONES[];  // C6, E6, G6
extern const unsigned long SUCCESS_DURATIONS[];
#define SUCCESS_LENGTH 3

// Error sound (descending)
extern const unsigned long ERROR_TONES[];  // G6, E6, G5
extern const unsigned long ERROR_DURATIONS[];
#define ERROR_LENGTH 3

// Click sound
extern const unsigned long CLICK_TONES[];  // C7
extern const unsigned long CLICK_DURATIONS[];
#define CLICK_LENGTH 2

// Button sound
extern const unsigned long BUTTON_TONES[];
extern const unsigned long BUTTON_DURATIONS[];
#define BUTTON_LENGTH 2

// Intro sound (song)
extern const unsigned long INTRO_TONES[];  // G6, E6, G5
extern const unsigned long INTRO_DURATIONS[];
#define INTRO_LENGTH 12

// Testsound sound effect
#define THEME_LENGTH 207
extern const unsigned long THEMESOUND[];
extern const unsigned long THEME_DURATIONS[];

// Function prototypes
void tone(unsigned long frequency);
void InitSoundEffects(void);
void PlaySoundEffect(const unsigned long *tones, const unsigned long *durations, int length);
void UpdateSoundEffects(void);
void PlaySuccessSound(void);
void PlayErrorSound(void);
void PlayClickSound(void);
void PlayButtonSound(void);
void PlayIntroSound(void);
void StopSound(void);
void PlayThemeSoundLooped(void);
void StopThemeLoop(void);

// Function to convert milliseconds to system ticks
unsigned long msToDelayTicks(unsigned long ms);

#endif // SOUND_EFFECTS_H
