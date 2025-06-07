// Implementation file (sound_Effects.c)

#include "sound_Effects.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "rom.h"
#include "rom_map.h"
#include "timer.h"
#include "utils.h"
#include "prcm.h"

#define TIMER_CLOCK_HZ 80000000  // 80MHz system clock
#define MIN_PERIOD 50            // Minimum timer period for good resolution
#define MAX_PERIOD 65535         // Maximum 16-bit timer period
#define TARGET_PERIOD 4000


// Define the melody arrays
// Success sound (ascending)
const unsigned long SUCCESS_TONES[] = {1047, 1319, 1568}; // C6, E6, G6
const unsigned long SUCCESS_DURATIONS[] = {1, 1, 10};

const unsigned long INTRO_TONES[] = {100, 200, 400, 700, 900, 1100,
                                       1500, 2000, 2500, 3000, 3900, 4000}; // C6, E6, G6
const unsigned long INTRO_DURATIONS[] = {1, 1, 1, 1, 1,1, 1,1,1,1,1,1};

// Error sound (descending)
const unsigned long ERROR_TONES[] = {200, 300, 500}; // G6, E6, G5
const unsigned long ERROR_DURATIONS[] = {1, 1, 2};

// Click sound
const unsigned long CLICK_TONES[] = {400, 500};
const unsigned long CLICK_DURATIONS[] = {1,1};

// Button sound
const unsigned long BUTTON_TONES[] = {800, 700};
const unsigned long BUTTON_DURATIONS[] = {.5, 1};

// Testsound sound effect

// Testsound sound effect
#define THEME_LENGTH 207

const unsigned long THEME_TONES[] = {
       0,  294,  294,  294,  294,  100,    0,  494,
     494,  494,  494,    0,  294,  294,  294,  294,
       0,  494,  494,  494,  494,    0,  698,  698,
     698,  698,    0,  494,  494,  494,  494,    0,
     294,  294,  294,  294,    0,  494,  494,  494,
     494,    0,  294,  294,  294,  294,    0,  294,
     294,  294,  294,    0,  587,  587,  587,  587,
       0,  392,  294,  294,  294,  294,  392,  294,
       0,  392,  392,  392,  392,    0,  587,  587,
     587,  587,    0,  784,  784,  784,  784,    0,
     494,  494,  494,  494,    0,  392,  392,  392,
     392,    0,  494,  494,  494,  494,    0,  294,
     294,  294,  294,    0,  494,  494,  494,  494,
       0,  294,  294,  294,  294,    0,  494,  494,
     494,  494,    0,  294,  294,  294,  294,    0,
     494,  494,  494,  494,    0,  698,  698,  698,
     698,    0,  494,  494,  494,  494,    0,  294,
     294,  294,  294,    0,  494,  494,  494,  494,
       0,  294,  294,  294,  294,    0,  392,  392,
     392,  392,    0,  587,  587,  587,  587,    0,
     392,  294,  294,  294,  294,  392,    0,  294,
     294,  294,  294,    0,  587,  587,  587,  587,
       0,  784,  784,  784,  784,    0,  494,  494,
     494,  494,    0,  294,  294,  294,  294,    0,
     494,  494,  494,  494,    0,  392,  392,  392,
     392,    0,  494,  494,  494,  494,    0
};

const unsigned long THEME_DURATIONS[] = {
    1.200000, 1.200000, 1.200000, 1.200000, 1.000000, 0.200000, 0.800000, 1.200000, 1.200000, 1.200000, 1.200000, 12.200000,
    1.200000, 1.200000, 1.200000, 1.200000, 1.500000, 1.200000, 1.200000, 1.200000, 1.200000, 12.200000, 1.200000, 1.200000,
    1.200000, 1.200000, 0.500000, 1.200000, 1.200000, 1.200000, 1.200000, 0.500000, 1.200000, 1.200000, 1.200000, 1.200000,
    12.500000, 1.200000, 1.200000, 1.200000, 1.200000, 0.800000, 1.200000, 1.200000, 1.200000, 1.000000, 6.800000, 1.200000,
    1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 0.200000, 0.200000, 1.200000, 1.200000,
    1.200000, 0.500000, 0.200000, 0.200000, 6.500000, 1.200000, 1.200000, 1.200000, 1.200000, 1.000000, 1.200000, 1.200000,
    1.200000, 1.200000, 12.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000,
    0.200000, 1.200000, 1.200000, 1.200000, 1.200000, 12.500000, 1.200000, 1.200000, 1.200000, 1.200000, 0.800000, 1.200000,
    1.200000, 1.200000, 1.000000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 0.500000, 1.200000, 1.200000, 1.200000,
    1.200000, 0.800000, 1.200000, 1.200000, 1.200000, 1.200000, 12.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.500000,
    1.200000, 1.200000, 1.200000, 1.200000, 12.200000, 1.200000, 1.200000, 1.200000, 1.200000, 0.500000, 1.200000, 1.200000,
    1.200000, 1.200000, 0.500000, 1.200000, 1.200000, 1.200000, 1.200000, 12.800000, 1.200000, 1.200000, 1.200000, 1.000000,
    0.800000, 1.200000, 1.200000, 1.200000, 1.200000, 6.500000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000,
    1.200000, 1.200000, 1.200000, 0.200000, 0.200000, 1.200000, 1.200000, 1.200000, 0.800000, 0.200000, 6.500000, 1.200000,
    1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.000000, 12.200000, 1.200000, 1.200000, 1.200000,
    1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 1.200000, 0.200000, 1.200000, 1.200000, 1.200000, 1.200000, 12.800000,
    1.200000, 1.200000, 1.200000, 1.000000, 0.800000, 1.200000, 1.200000, 1.200000, 1.200000, 1.000000, 1.200000, 1.200000,
    1.200000, 1.200000, 2.000000
};

// Function to convert milliseconds to UtilsDelay ticks
// UtilsDelay is a 3-instruction loop, so each loop iteration takes 3 cycles
unsigned long msToDelayTicks(unsigned long ms)
{
    // 80MHz = 80,000,000 cycles per second
    // Each UtilsDelay loop takes 3 cycles
    // For 1ms delay at 80MHz, we need 80,000 cycles
    // Therefore, for 1ms we need 80,000/3 = 26,667 iterations

    // Reorder operations to avoid overflow: (80,000/3) * ms
    return (80000 / 3) * ms;
}

// Get system time in milliseconds (approximation using SysTickValueGet)
unsigned long getTimeMs()
{
    // This is a simple approach that just counts how many times we've called the function
    // It's not accurate real time, but it's enough to measure note durations
    static unsigned long counter = 0;
    return counter++;
}

static unsigned long CalculateOptimalPrescaler(unsigned long frequency)
{
    unsigned long raw_period;
    unsigned long prescaler;
    unsigned long best_prescaler = 0;
    unsigned long best_error = 0xFFFFFFFF;
    unsigned long test_period, actual_freq, error;
    unsigned long prescaled_clock;

    if (frequency == 0) {
        return 0;
    }

    // Calculate what the period would be with no prescaling
    raw_period = TIMER_CLOCK_HZ / frequency;

    // If raw period fits nicely, use no prescaling
    if (raw_period >= MIN_PERIOD && raw_period <= MAX_PERIOD) {
        return 0;  // No prescaling needed
    }

    // Search for the best prescaler (test prescalers 0-255)
    for (prescaler = 0; prescaler <= 255; prescaler++) {
        prescaled_clock = TIMER_CLOCK_HZ / (prescaler + 1);
        test_period = prescaled_clock / frequency;

        // Skip if period is out of range
        if (test_period < MIN_PERIOD || test_period > MAX_PERIOD) {
            continue;
        }

        // Calculate actual frequency and error
        actual_freq = prescaled_clock / test_period;
        if (actual_freq > frequency) {
            error = actual_freq - frequency;
        } else {
            error = frequency - actual_freq;
        }

        // Keep track of the prescaler with minimum error
        if (error < best_error) {
            best_error = error;
            best_prescaler = prescaler;
        }

        // If we found exact match, use it
        if (error == 0) {
            break;
        }
    }

    return best_prescaler;
}

// Function to set the frequency of the PWM for tone generation
void tone(unsigned long frequency)
{
    unsigned long ulPeriod;
        unsigned long ulPrescaler;
        unsigned long ulPrescaledClock;
        unsigned long actualFrequency;

        ulPrescaler = CalculateOptimalPrescaler(frequency);

        // Calculate the prescaled clock frequency
        ulPrescaledClock = TIMER_CLOCK_HZ / (ulPrescaler + 1);

        // Calculate the period for the prescaled clock
        ulPeriod = ulPrescaledClock / frequency;

        // Calculate what the actual frequency will be
        actualFrequency = ulPrescaledClock / ulPeriod;

        // Debug output - this will show you the discrepancy
        Report("\r\nRequested: %lu Hz, Prescaler: %lu, Period: %lu, Actual: %lu Hz",
               frequency, ulPrescaler, ulPeriod, actualFrequency);

        // Make sure the period fits in 16 bits
        if(ulPeriod > 65535)
        {
            ulPeriod = 65535;
            Report("\r\nWarning: Period clamped to 65535");
        }

        // Set the prescaler
        MAP_TimerPrescaleSet(TIMERA2_BASE, TIMER_B, ulPrescaler);
        // Set the timer load value (period)
        MAP_TimerLoadSet(TIMERA2_BASE, TIMER_B, ulPeriod);
        // Set 50% duty cycle for clean tone
        MAP_TimerMatchSet(TIMERA2_BASE, TIMER_B, ulPeriod / 2);
}

// Initialize the PWM for buzzer
void InitSoundEffects()
{
    // Enable the PWM peripheral clock
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA2, PRCM_RUN_MODE_CLK);

    // Configure the timer for PWM mode
    MAP_TimerConfigure(TIMERA2_BASE, (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_B_PWM));

    // Inverting the timer output for active-high PWM
    MAP_TimerControlLevel(TIMERA2_BASE, TIMER_B, 1);

    // Initialize with no tone
    MAP_TimerLoadSet(TIMERA2_BASE, TIMER_B, 1000);
    MAP_TimerMatchSet(TIMERA2_BASE, TIMER_B, 0);

    // Enable the timer
    MAP_TimerEnable(TIMERA2_BASE, TIMER_B);

    // Initialize sound state
    g_SoundState.playing = false;
    g_SoundState.looping = false;
}

// Start playing a sound effect (non-blocking)
// Start playing a sound effect (non-blocking)
void PlaySoundEffect(const unsigned long *tones, const unsigned long *durations, int length)
{
    // If a looped theme is playing, don't allow other sounds to interrupt it
    if(g_SoundState.playing && g_SoundState.looping)
    {
        // Only allow the theme to restart itself
        if(tones != THEME_TONES)
        {
            return; // Ignore non-theme sounds while theme is looping
        }
    }

    // If already playing, stop the current sound
    if(g_SoundState.playing)
    {
        tone(0); // Stop current tone
    }

    // Set up the new sound effect
    g_SoundState.tones = tones;
    g_SoundState.durations = durations;
    g_SoundState.melodyLength = length;
    g_SoundState.currentNoteIndex = 0;
    g_SoundState.playing = true;

    // Get current time
    g_SoundState.noteStartTime = getTimeMs();

    // Start playing the first note
    tone(tones[0]);
}

// Update sound playback - call this from main loop
void UpdateSoundEffects()
{
    if(!g_SoundState.playing)
    {
        return; // Nothing to do if not playing
    }

    // Get current time
    unsigned long currentTime = getTimeMs();

    // Check if it's time for the next note
    if(currentTime - g_SoundState.noteStartTime >= g_SoundState.durations[g_SoundState.currentNoteIndex])
    {
        // Move to next note
        g_SoundState.currentNoteIndex++;

        // Check if we reached the end of the melody
        if(g_SoundState.currentNoteIndex >= g_SoundState.melodyLength)
        {
            // If looping is enabled, restart the melody
            if(g_SoundState.looping)
            {
                g_SoundState.currentNoteIndex = 0;
                g_SoundState.noteStartTime = currentTime;
                tone(g_SoundState.tones[0]);
                return;
            }

            // End of sound effect
            g_SoundState.playing = false;
            g_SoundState.looping = false;  // Clear looping flag
            tone(0); // Stop sound
            return;
        }

        // Update start time for next note
        g_SoundState.noteStartTime = currentTime;

        // Play the next note
        tone(g_SoundState.tones[g_SoundState.currentNoteIndex]);
    }
}

// Convenience functions for common sound effects
void PlaySuccessSound(void)
{
    PlaySoundEffect(SUCCESS_TONES, SUCCESS_DURATIONS, SUCCESS_LENGTH);
}

void PlayErrorSound(void)
{
    PlaySoundEffect(ERROR_TONES, ERROR_DURATIONS, ERROR_LENGTH);
}

void PlayClickSound(void)
{
    PlaySoundEffect(CLICK_TONES, CLICK_DURATIONS, CLICK_LENGTH);
}

void PlayButtonSound(void)
{
    PlaySoundEffect(BUTTON_TONES, BUTTON_DURATIONS, BUTTON_LENGTH);
}

void PlayIntroSound(void)
{
    PlaySoundEffect(INTRO_TONES, INTRO_DURATIONS, INTRO_LENGTH);
}

// Function to play the theme song for videogame
void PlayThemeSoundLooped(void)
{
    PlaySoundEffect(THEME_TONES, THEME_DURATIONS, THEME_LENGTH);
    g_SoundState.looping = true;
    g_SoundState.playing = true;
}
 //function to stop the looped theme song
void StopThemeLoop(void)
{
    g_SoundState.looping = false;
    StopSound();
}

// Stop any currently playing sound
void StopSound(void)
{
    g_SoundState.playing = false;
    tone(0); // Turn off PWM
}
