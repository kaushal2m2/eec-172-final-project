//*****************************************************************************
// Oscilloscope Interface for Joystick X-Axis
// Displays real-time voltage readings from the joystick's X-axis
//*****************************************************************************

#include <shared_defs.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "interrupt.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "pinmux.h"
#include "pin.h"
#include "adc.h"
#include "hw_adc.h"
#include "uart_if.h"
#include "timer.h"
#include "hw_timer.h"
#include "oscilloscope_bitmap.h"
#include "systick.h"

// App includes
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"

// UI settings
#define SCREEN_WIDTH         128
#define SCREEN_HEIGHT        128

// Oscilloscope settings
#define SCOPE_HEIGHT         65   // Height of oscilloscope display
#define SCOPE_TOP            20   // Top position of oscilloscope display
#define SCOPE_BUFFER_SIZE    128  // Buffer size (matches screen width)
#define SCOPE_COLOR          CYAN // Color for oscilloscope trace
#define GRID_COLOR           0x4208 // Dimmed white for grid lines
#define MAX_VOLTAGE          1.4  // Maximum expected voltage

// Colors
#define TEXT_COLOR           WHITE
#define BACKGROUND_COLOR     BLACK
#define INDICATOR_COLOR      RED

// Button 2 pin for exit detection
#define BUTTON2_PIN          0x20     // PIN_21
#define BUTTON2_PORT         GPIOA1_BASE

// Joystick reading constants

#define JOYSTICK_CENTER      0.7      // Center voltage (neutral position)
#define JOYSTICK_THRESHOLD   0.1      // Threshold for detecting movement
#define JOYSTICK_HIGH        (JOYSTICK_CENTER + JOYSTICK_THRESHOLD)
#define JOYSTICK_LOW         (JOYSTICK_CENTER - JOYSTICK_THRESHOLD)

// Sampling rates
#define DISPLAY_REFRESH_RATE 58.8    // Hz - for display updates
#define BATCH_SAMPLING_RATE 49400.0  // Hz - estimated for batch sampling

// Application state variables
static bool g_initialized = false;    // Initialization flag

// Oscilloscope buffer
static float g_voltageBuffer[SCOPE_BUFFER_SIZE];
static bool g_bufferComplete = false;  // Flag to indicate complete buffer


static int g_previous_trace_y[SCOPE_BUFFER_SIZE];  // Store previous Y coordinates
static bool g_previous_trace_valid = false;        // Flag if previous trace exists


// Forward declarations
static void BatchSampleBuffer(void);
static void DrawOscilloscope(void);
static bool ShouldExit(void);
static unsigned long g_batchSampleTicks = 0;


// Timestep constants and parameter
static int timeStep = 10;
#define MAX_TIMESTEP 10000
#define MIN_TIMESTEP 10
#define TIMESTEP_STEP 100
#define TIMER_FREQ_HZ 80000000.0f


// voltagestep constants and parameter
static float voltageStep = 1.40;
#define MAX_VOLTAGESTEP 3
#define MIN_VOLTAGESTEP .1
#define VOLTAGESTEP_STEP .1

//*****************************************************************************
// Initialize the application
//*****************************************************************************
void Oscilloscope_Initialize(void)
{
    // First, disable SysTick to safely reconfigure
    SysTickDisable();

    // Disable SysTick interrupt to avoid conflicts
    SysTickIntDisable();

    SysTickPeriodSet(0xFFFFFF);  // 24-bit max value
    SysTickEnable();
    // Initialize voltage buffer with zeros
    int i = 0;
    for (i = 0; i < SCOPE_BUFFER_SIZE; i++) {
        g_voltageBuffer[i] = 0.0;
    }

    // Clear the screen
    const uint8_t* oscilloscope_frame_bitmap = get_oscilloscope_frame(0);
    fastDrawBitmap(0, 0, oscilloscope_frame_bitmap, OSCILLOSCOPE_WIDTH, OSCILLOSCOPE_HEIGHT, GREEN, BLACK, 1);

    // Draw the initial oscilloscope
    DrawOscilloscope();

    // Wait for initialization
    MAP_UtilsDelay(8000000);

    g_initialized = true;
}

//*****************************************************************************
// Batch sample entire buffer quickly
//*****************************************************************************
static void BatchSampleBuffer(void)
{
    int i = 0;
    unsigned long ulSample;
    unsigned long startTicks, endTicks, elapsedTicks;
    startTicks = SysTickValueGet();

    // Enable ADC channel once
    MAP_ADCChannelEnable(ADC_BASE, ADC_CH_1);

    // Sample entire buffer rapidly
    for (i = 0; i < SCOPE_BUFFER_SIZE; i++) {
        // Wait for sample to be ready and read it
        while(!MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_1));
        ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_1);

        // Convert to voltage (single sample, no averaging for speed)
        g_voltageBuffer[i] = (((float)((ulSample >> 2) & 0x0FFF)) * 1.4) / 4096;
        MAP_UtilsDelay(timeStep);
    }

    // Disable ADC channel once
    MAP_ADCChannelDisable(ADC_BASE, ADC_CH_1);

    endTicks = SysTickValueGet();

    if (startTicks >= endTicks) {
            elapsedTicks = startTicks - endTicks;
    } else {
            elapsedTicks = 0;
    }

    g_batchSampleTicks = elapsedTicks;

    g_bufferComplete = true;
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
// Read joystick Y-axis position
//*****************************************************************************
static float ReadJoystickY(void)
{
    unsigned int uiIndex = 0;
        unsigned long ulSample;
        float avgVoltage = 0.0;

        // Enable ADC channel
        MAP_ADCChannelEnable(ADC_BASE, ADC_CH_3);

        // Sample 10 times and average
        uiIndex = 0;
        while(uiIndex < 10)
        {
            if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_3))
            {
                ulSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_3);
                // Calculate voltage: (ADC value * 1.4V reference) / 4096 (12-bit resolution)
                avgVoltage += (((float)((ulSample >> 2) & 0x0FFF)) * 1.4) / 4096;
                uiIndex++;
            }
        }

        // Disable ADC channel
        MAP_ADCChannelDisable(ADC_BASE, ADC_CH_3);

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
    float joystick_y = ReadJoystickY();
    //Report("\r\%f", joystick_x);
    // Debouncing - only check every few frames
    debounce_counter++;
    if (debounce_counter < 5) {
        return;
    }
    debounce_counter = 0;

    if (joystick_x < JOYSTICK_LOW) {
        // Joystick moved right - increase frequency
        if (timeStep < MAX_TIMESTEP) {
            // Analog adjustment
            timeStep += TIMESTEP_STEP* ((.7 - joystick_x)/.7);
            if (timeStep > MAX_TIMESTEP) {
                timeStep = MAX_TIMESTEP;
            }
        }
    } else if (joystick_x > JOYSTICK_HIGH) {
        // Joystick moved left - decrease frequency
        if (timeStep > MIN_TIMESTEP) {
            // Analog adjustment
            timeStep -= TIMESTEP_STEP * ((joystick_x-.7)/.7);
            if (timeStep < MIN_TIMESTEP) {
                timeStep = MIN_TIMESTEP;
            }
        }
    }

    if (joystick_y < JOYSTICK_LOW) {
            // Joystick moved right - increase frequency
            if (voltageStep < MAX_VOLTAGESTEP) {
                // Analog adjustment
                voltageStep += VOLTAGESTEP_STEP;
                if (voltageStep > MAX_VOLTAGESTEP) {
                    voltageStep = MAX_VOLTAGESTEP;
                }
            }
        } else if (joystick_y > JOYSTICK_HIGH) {
            // Joystick moved left - decrease frequency
            if (voltageStep > MIN_VOLTAGESTEP) {
                // Analog adjustment
                voltageStep -= VOLTAGESTEP_STEP;
                if (voltageStep < MIN_VOLTAGESTEP) {
                    voltageStep = MIN_VOLTAGESTEP;
                }
            }
        }
}

//*****************************************************************************
// Extract frequency using zero-crossing detection
//*****************************************************************************
float ExtractFrequency_ZeroCrossing(void)
{
    float frequency = 0;
    float threshold = MAX_VOLTAGE / 5.0f;
    int crossings = 0;
    bool above_threshold = (g_voltageBuffer[0] > threshold);
    int i = 0;
    float actual_time_span;
    float actual_sampling_rate;

    // Safety check - ensure we have valid timing data
    if (g_batchSampleTicks == 0) {
        return 0.0f;  // Avoid division by zero
    }

    // Count zero crossings in the buffer
    for (i = 1; i < SCOPE_BUFFER_SIZE; i++) {
        bool current_above = (g_voltageBuffer[i] > threshold);
        if (current_above != above_threshold) {
            crossings++;
            above_threshold = current_above;
        }
    }

    // Calculate the actual time span of the sampling period
    // g_batchSampleTicks contains timer ticks for the entire BatchSampleBuffer() execution
    actual_time_span = (float)g_batchSampleTicks / TIMER_FREQ_HZ;

    // Calculate the actual sampling rate
    actual_sampling_rate = (float)SCOPE_BUFFER_SIZE / actual_time_span;

    // Calculate frequency: (complete cycles) / (time span)
    // Two crossings = one complete cycle
    frequency = (crossings / 2.0f) / actual_time_span;

    //weird experimental adjustments to account for the properties of the breadboard
    if(frequency  <= 700){frequency += 100;}
    else if(frequency <= 795){frequency += 200;}
    else if(frequency <= 820){frequency +=300;}
    else if(frequency <= 900){frequency +=400;}

    return frequency;
}

//*****************************************************************************
// Find both minimum and maximum voltages in one pass (more efficient)
// Parameters: pointers to store min and max values
//*****************************************************************************
static void GetMinMaxVoltage(float* min_voltage, float* max_voltage)
{
    int i;

    /* Initialize with first buffer value */
    *min_voltage = g_voltageBuffer[0];
    *max_voltage = g_voltageBuffer[0];

    /* Search through entire buffer */
    for (i = 1; i < SCOPE_BUFFER_SIZE; i++) {
        if (g_voltageBuffer[i] > *max_voltage) {
            *max_voltage = g_voltageBuffer[i];
        }
        if (g_voltageBuffer[i] < *min_voltage) {
            *min_voltage = g_voltageBuffer[i];
        }
    }
}

//*****************************************************************************
// Check if button 2 is pressed to exit the application
//*****************************************************************************
static bool ShouldExit(void)
{
    // Read the current button state (active low)
    return !(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0);
}

//*****************************************************************************
// Draw the oscilloscope interface for X-axis voltage readings
//*****************************************************************************
static void DrawOscilloscope(void)
{
    char buffer[32];
    int i = 0;
    float frequency = 0;
    float min_voltage, max_voltage;
    float peakToPeak;

    // Draw static elements only once
    static bool static_elements_drawn = false;

    // Draw voltage scale labels
    sprintf(buffer, "%.2f", voltageStep);
    Outstr(buffer, GREEN, BLACK, 12, 27, 128, 50);
    Outstr("0V", GREEN, BLACK, 12, 86, 118, 115);
    GetMinMaxVoltage(&min_voltage, &max_voltage);

    // Printing Peak Voltage
    sprintf(buffer, "%.2f", max_voltage);
    Outstr(buffer, GREEN, BLACK, 31, 102, 80, 120);

    // Printing Peak to Peak voltage
    peakToPeak = max_voltage - min_voltage;
    sprintf(buffer, "%.2f", peakToPeak);
    Outstr(buffer, GREEN, BLACK, 48, 112, 85, 128);

    // STEP 1: Erase previous trace by drawing it in black
    if (g_previous_trace_valid) {
        for (i = 11; i < SCOPE_BUFFER_SIZE - 4; i++) {
            int x1 = i - 1;
            int x2 = i;
            int y1 = g_previous_trace_y[i - 1];
            int y2 = g_previous_trace_y[i];

            // Draw previous trace line in black to erase it
            drawLine(x1, y1, x2, y2, BACKGROUND_COLOR);
        }
    }

    // STEP 3: Calculate and store new trace coordinates
    if (g_bufferComplete) {
        for (i = 0; i < SCOPE_BUFFER_SIZE; i++) {
            // Map voltage (0-1.4V) to y position (SCOPE_TOP+SCOPE_HEIGHT to SCOPE_TOP)
            int y = SCOPE_TOP + SCOPE_HEIGHT - (int)((g_voltageBuffer[i] / voltageStep) * SCOPE_HEIGHT);

            // Clamp values to stay within the oscilloscope area
            if (y < SCOPE_TOP) y = SCOPE_TOP;
            if (y > SCOPE_TOP + SCOPE_HEIGHT) y = SCOPE_TOP + SCOPE_HEIGHT;

            // Store y coordinate for this x position
            g_previous_trace_y[i] = y;
        }

        // STEP 4: Draw new trace in scope color
        for (i = 11; i < SCOPE_BUFFER_SIZE - 4; i++) {
            int x1 = i - 1;
            int x2 = i;
            int y1 = g_previous_trace_y[i - 1];
            int y2 = g_previous_trace_y[i];

            // Draw new trace line
            drawLine(x1, y1, x2, y2, SCOPE_COLOR);
        }

        // Mark that we now have a valid previous trace for next frame
        g_previous_trace_valid = true;
    }

    // Draw frequency. 4.94 found experimentally
    frequency = ExtractFrequency_ZeroCrossing();
    sprintf(buffer, "%.0fHz       ", frequency);
    if(frequency != 0){
        Outstr(buffer, GREEN, BLACK, 26, 121, 128, 128);
    }

    // Draw Timestep
    sprintf(buffer, "%.d", timeStep);
    Outstr(buffer, GREEN, BLACK, 110, 101, 128, 128);

    sprintf(buffer, "%.2f", voltageStep);
    Outstr(buffer, GREEN, BLACK, 76, 119, 128, 128);

}

//*****************************************************************************
// Run one frame of the application
//*****************************************************************************

bool Oscilloscope_RunFrame(void)
{
    if (!g_initialized) {
        Oscilloscope_Initialize();
        return true;
    }

    // Check if button 2 is pressed to exit
    if (ShouldExit()) {
        return false;
    }

    // Batch sample entire buffer quickly
    BatchSampleBuffer();

    // Read Joystick input and change scale accordingly
    HandleJoystickInput();

    // Update oscilloscope display once per complete buffer
    DrawOscilloscope();

    return true;
}

//*****************************************************************************
// Clean up resources before exiting
//*****************************************************************************
void Oscilloscope_Cleanup(void)
{
    // Clear the screen
    fillScreen(BACKGROUND_COLOR);
    g_initialized = false;
}
