//*****************************************************************************
// Function Generator with Integrated Square Wave Visualizer
// Uses existing sound_Effects.c tone() function for signal generation
// Includes real-time square wave visualization
//*****************************************************************************
#include <shared_defs.h>
#include "functiongenerator.h"
#include "sound_Effects.h"  // Import the working sound effects module
#include "hw_types.h"
#include "utils.h"
#include "uart_if.h"
#include "rom.h"
#include "rom_map.h"
#include "timer.h"
#include <math.h>
#include "adc.h"
#include "hw_adc.h"
#include "prcm.h"
#include "interrupt.h"
#include "hw_memmap.h"
#include "timer.h"
#include "hw_timer.h"
#include "funcgenerator_bitmap.h"


// Display includes
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "pin.h"

// UI settings
#define SCREEN_WIDTH         128
#define SCREEN_HEIGHT        128

// Visualizer settings
#define SCOPE_HEIGHT         80   // Height of waveform display
#define SCOPE_TOP            20   // Top position of waveform display
#define SCOPE_BUFFER_SIZE    128  // Buffer size (matches screen width)
#define SCOPE_COLOR          YELLOW // Color for waveform trace
#define GRID_COLOR           GREEN // Dimmed white for grid lines
#define MAX_AMPLITUDE        1.0  // Maximum waveform amplitude

// Colors
#define TEXT_COLOR           WHITE
#define BACKGROUND_COLOR     BLACK
#define FREQUENCY_COLOR      YELLOW
#define STATUS_COLOR         RED

// Button 2 pin for exit detection
#define BUTTON2_PIN          0x20     // PIN_21
#define BUTTON2_PORT         GPIOA1_BASE

// Joystick control settings
#define JOYSTICK_CENTER      0.7      // Center voltage (neutral position)
#define JOYSTICK_THRESHOLD   0.2      // Threshold for detecting movement
#define JOYSTICK_HIGH        (JOYSTICK_CENTER + JOYSTICK_THRESHOLD)
#define JOYSTICK_LOW         (JOYSTICK_CENTER - JOYSTICK_THRESHOLD)
#define FREQUENCY_STEP       100       // Frequency change step in Hz
#define MIN_FREQUENCY        10       // Minimum frequency
#define MAX_FREQUENCY        3000     // Maximum frequency

//*****************************************************************************
// Function Generator Parameters - Modify these as needed
//*****************************************************************************
static unsigned long g_frequency = 1000;    // Output frequency in Hz (1kHz default)
static bool g_initialized = false;
static bool g_enabled = false;
static bool g_play_signal = false;


// Display variables
static bool screenNeedsUpdate = true;
static float g_waveformBuffer[SCOPE_BUFFER_SIZE];
static int g_previous_trace_y[SCOPE_BUFFER_SIZE];  // Store previous Y coordinates
static bool g_previous_trace_valid = false;        // Flag if previous trace exists
static char g_previous_freq_text[16] = "";         // Previous frequency text
static bool g_display_initialized = false;
static bool button1Input = false;
static int debounceCounter = 0;

// Button 2 pin for exit detection
#define BUTTON2_PIN             0x20     // PIN_21
#define BUTTON2_PORT            GPIOA1_BASE

// Button 1 pin for jumping
#define BUTTON1_PIN             0x40    // PIN_15
#define BUTTON1_PORT            GPIOA2_BASE


//*****************************************************************************
// Forward declarations
//*****************************************************************************
static void GenerateSquareWaveBuffer(unsigned long frequency);
static void DrawWaveformDisplay(void);
static float ReadJoystickX(void);
static void HandleJoystickInput(void);

//*****************************************************************************
// Initialize the Function Generator with Display
//*****************************************************************************
void FunctionGenerator_Initialize(void)
{
    // Initialize waveform buffer
    int i = 0;
    for (i = 0; i < SCOPE_BUFFER_SIZE; i++) {
        g_waveformBuffer[i] = 0.0;
    }

    const uint8_t* funcgenerator_frame_bitmap = get_funcgenerator_frame(0);
    fastDrawBitmap(0, 0, funcgenerator_frame_bitmap, FUNCGENERATOR_WIDTH, FUNCGENERATOR_HEIGHT, GREEN, BLACK, 1);

    g_initialized = true;
    g_enabled = false;
    screenNeedsUpdate = true;

    Report("Function Generator with Visualizer Initialized\n\r");


    // Small delay for initialization
    MAP_UtilsDelay(8000);
}

//*****************************************************************************
// Generate square wave buffer for visualization
//*****************************************************************************
static void GenerateSquareWaveBuffer(unsigned long frequency)
{
    if (frequency == 0) {
        // Generate flat line for zero frequency
        int i = 0;
        for (i = 0; i < SCOPE_BUFFER_SIZE; i++) {
            g_waveformBuffer[i] = 0.0;
        }
        return;
    }

    // Calculate phase increment per sample for continuous phase
    // Normalize frequency to 200Hz = 1 complete cycle across screen
    float normalized_frequency = frequency / 200.0;
    float phase_increment = (2.0 * M_PI * normalized_frequency) / SCOPE_BUFFER_SIZE;

    int i = 0;
    for (i = 0; i < SCOPE_BUFFER_SIZE; i++) {
        // Calculate continuous phase for this sample
        float phase = i * phase_increment;

        // Normalize phase to 0-2pi range
        float normalized_phase = fmod(phase, 2.0 * M_PI);

        // Square wave: high for first half of cycle (0 to pi), low for second half (pi to 2pi)
        if (normalized_phase < M_PI) {
            g_waveformBuffer[i] = MAX_AMPLITUDE;   // High state
        } else {
            g_waveformBuffer[i] = -MAX_AMPLITUDE;  // Low state
        }
    }
}


//*****************************************************************************
// Read joystick X-axis position
//*****************************************************************************
static float ReadJoystickX(void)
{
    unsigned int uiIndex = 0;
        unsigned long ulSample;
        float avgVoltage = 0.0;

        // Enable ADC channel
        MAP_ADCChannelEnable(ADC_BASE, ADC_CH_2);

        // Sample 10 times and average
        uiIndex = 0;
        while(uiIndex < 10)
        {
            if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_2))
            {
                ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_2);
                // Calculate voltage: (ADC value * 1.4V reference) / 4096 (12-bit resolution)
                avgVoltage += (((float)((ulSample >> 2) & 0x0FFF)) * 1.4) / 4096;
                uiIndex++;
            }
        }

        // Disable ADC channel
        MAP_ADCChannelDisable(ADC_BASE, ADC_CH_2);

        // Calculate average voltage
        return (avgVoltage / 10.0);
}

//*****************************************************************************
// Handle joystick input for frequency control
//*****************************************************************************
static void HandleJoystickInput(void)
{
    static unsigned long debounce_counter = 0;

    // Read current joystick position
    float joystick_x = ReadJoystickX();
    Report("\r\%f", joystick_x);
    // Debouncing - only check every few frames
    debounce_counter++;
    if (debounce_counter < 5) {
        return;
    }
    debounce_counter = 0;

    // Detect joystick movement and switch to manual mode
    bool joystick_moved = false;

    if (joystick_x < JOYSTICK_LOW) {
        // Joystick moved right - increase frequency
        if (g_frequency < MAX_FREQUENCY) {
            g_frequency += FREQUENCY_STEP;
            if (g_frequency > MAX_FREQUENCY) {
                g_frequency = MAX_FREQUENCY;
            }
            joystick_moved = true;
        }
    } else if (joystick_x > JOYSTICK_HIGH) {
        // Joystick moved left - decrease frequency
        if (g_frequency > MIN_FREQUENCY) {
            g_frequency -= FREQUENCY_STEP;
            if (g_frequency < MIN_FREQUENCY) {
                g_frequency = MIN_FREQUENCY;
            }
            joystick_moved = true;
        }
    }
    screenNeedsUpdate = joystick_moved;
}

//*****************************************************************************
// Draw the waveform display
//*****************************************************************************
static void DrawWaveformDisplay(void)
{

    if(screenNeedsUpdate){

        // Reset ScreenNeedsUpdate so dont redraw until next joystick movement
        screenNeedsUpdate = false;
        char buffer[32];
        int i = 0;
        // Draw amplitude labels. Could use a bool flag for these if optimizing but its so fast I don't care.
        setCursor(2, SCOPE_TOP - 8);
        Outstr("+", GREEN, BLACK, 2, SCOPE_TOP - 8, 5, SCOPE_TOP);
        Outstr("0", GREEN, BLACK, 2, SCOPE_TOP + SCOPE_HEIGHT/2 - 4, 5, SCOPE_TOP + SCOPE_HEIGHT/2);
        Outstr("-", GREEN, BLACK, 2, SCOPE_TOP + SCOPE_HEIGHT - 8, 5, SCOPE_TOP + SCOPE_HEIGHT);

        // Erase previous trace
        if (g_previous_trace_valid) {
            for (i = 1; i < SCOPE_BUFFER_SIZE; i++) {
                drawLine(i-1, g_previous_trace_y[i-1], i, g_previous_trace_y[i], BACKGROUND_COLOR);
            }
        }

        //Draw Grid Lines (horizontal)
        for (i = 0; i <= 4; i++) {
            int y = SCOPE_TOP + (i * (SCOPE_HEIGHT / 4));
            drawFastHLine(9, y, 124, GRID_COLOR);
        }

        // Draw grid lines (vertical)
        for (i = 0; i <= 6; i++) {
            int x = (i * 16) + 8;
            drawFastVLine(x, SCOPE_TOP, SCOPE_HEIGHT, GRID_COLOR);
        }

        // Calculate and store new trace coordinates
        for (i = 0; i < SCOPE_BUFFER_SIZE; i++) {
            // Map amplitude (-1 to +1) to y position
            int y = SCOPE_TOP + SCOPE_HEIGHT/2 - (int)((g_waveformBuffer[i] / MAX_AMPLITUDE) * (SCOPE_HEIGHT/2));

            // Clamp to display area
            if (y < SCOPE_TOP) y = SCOPE_TOP;
            if (y > SCOPE_TOP + SCOPE_HEIGHT) y = SCOPE_TOP + SCOPE_HEIGHT;

            g_previous_trace_y[i] = y;
        }

        // Draw new trace
        for (i = 9; i < SCOPE_BUFFER_SIZE; i++) {
            drawLine(i-1, g_previous_trace_y[i-1], i, g_previous_trace_y[i], SCOPE_COLOR);
        }
        g_previous_trace_valid = true;

        //Clear prev text
        fillRect(44, 103, 37, 20, BLACK);
        fillRect(108, 105, 19, 20, BLACK);

        // Draw frequency display
        if (g_frequency >= 1000) {
            sprintf(buffer, "%.1fkHz", g_frequency / 1000.0);
        } else {
            sprintf(buffer, "%luHz", g_frequency);
        }

        Outstr(buffer, GREEN, BLACK, 44, 105, 81, 118);
        strcpy(g_previous_freq_text, buffer);


        if (g_play_signal) {
            Outstr("ON", GREEN, BLACK, 108, 107, 128, 128);
        } else {
            Outstr("OFF", GREEN, BLACK, 108, 107, 128, 128);
        }

    }
}

//*****************************************************************************
// Set Function Generator Frequency
//*****************************************************************************
void FunctionGenerator_SetFrequency(unsigned long frequency)
{
    if (!g_initialized) {
        return;
    }

    g_frequency = frequency;

    if (g_enabled) {
        // Use the working tone() function to set frequency
        tone(frequency);
        Report("Frequency set to: %lu Hz\n\r", frequency);
    }
}

//*****************************************************************************
// Enable/Disable Function Generator Output
//*****************************************************************************
void FunctionGenerator_Enable(bool enable)
{
    if (!g_initialized) {
        return;
    }

    if (enable) {
        // Enable output with current frequency using tone()
        tone(g_frequency);
        g_enabled = true;
        Report("Function Generator Enabled - %lu Hz\n\r", g_frequency);
    } else {
        // Disable output by calling tone(0)
        g_enabled = false;
        Report("Function Generator Disabled\n\r");
    }
}

//*****************************************************************************
// Run one frame of the function generator with visualization
//*****************************************************************************
bool FunctionGenerator_RunFrame(void)
{
    if (!g_initialized) {
        FunctionGenerator_Initialize();
        return true;
    }


    // Enable output if not already enabled
    if (!g_enabled) {
        FunctionGenerator_Enable(true);
    }

    HandleJoystickInput();
    // Generate square wave visualization data
    GenerateSquareWaveBuffer(g_frequency);

    // Update display
    DrawWaveformDisplay();

    //Toggle for signal
    button1Input = !(GPIOPinRead(BUTTON1_PORT, BUTTON1_PIN) == 0);
    if(button1Input & g_play_signal & (debounceCounter >= 20)){
        g_play_signal = false;
        debounceCounter = 0;
        screenNeedsUpdate = true;
    }
    else if(button1Input & (debounceCounter >= 20)){
        g_play_signal = true;
        debounceCounter = 0;
        screenNeedsUpdate = true;
    }

    if(debounceCounter <= 200) {debounceCounter++;};

    FunctionGenerator_PlayFrequency();

    // Minimal delay for display refresh
    MAP_UtilsDelay(50000);

    return true;  // Continue running
}

//*****************************************************************************
// Clean up function generator resources
//*****************************************************************************
void FunctionGenerator_Cleanup(void)
{
    if (g_initialized) {
        // Clear display
        if (g_display_initialized) {
            fillScreen(BACKGROUND_COLOR);
        }
        screenNeedsUpdate = true;
        g_initialized = false;
        Report("Function Generator Cleanup Complete\n\r");
    }
}

//*****************************************************************************
// Get current function generator status
//*****************************************************************************
bool FunctionGenerator_IsEnabled(void)
{
    return g_play_signal;
}

//*****************************************************************************
// Get current frequency setting
//*****************************************************************************
unsigned long FunctionGenerator_GetFrequency(void)
{
    return g_frequency;
}

//*****************************************************************************
// Quick frequency change function - immediate effect
//*****************************************************************************
void FunctionGenerator_PlayFrequency()
{
    if (g_play_signal) {
        tone(g_frequency);  // Immediate frequency change
        g_enabled = (g_frequency > 0);
        Report("Playing: %lu Hz\n\r", g_frequency);
    }
    else{
        tone(0);
    }
}

//*****************************************************************************
// Stop output immediately
//*****************************************************************************
void FunctionGenerator_Stop(void)
{
    if (g_initialized) {
        tone(0);  // Stop immediately
        g_enabled = false;
        Report("Function Generator Stopped\n\r");
    }
}
