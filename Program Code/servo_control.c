//*****************************************************************************
// Servo Control Interface with 3D Arm Visualization
// Controls two servos based on X and Y axis joystick readings
// Displays a 3D wireframe rectangle representing servo 1 arm position
//*****************************************************************************

#include <shared_defs.h>
#include <math.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "interrupt.h"
#include "rom.h"
#include "rom_map.h"
#include "timer.h"
#include "utils.h"
#include "prcm.h"
#include "pinmux.h"
#include "pin.h"
#include "adc.h"
#include "hw_adc.h"
#include "uart_if.h"
#include "servoarm_bitmap.h"

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"

// Digital Servo Settings
#define SERVO_FREQ_HZ 300       // 300Hz for digital servo (instead of 50Hz)
#define SERVO_PERIOD_US 3333    // 1000000/300 = 3333.33us period
#define SERVO_MIN_US 1000       // 1000us = 0 degrees
#define SERVO_MID_US 1500       // 1500us = 90 degrees
#define SERVO_MAX_US 2000       // 2000us = 180 degrees

// Button 2 pin for exit detection
#define BUTTON2_PIN          0x20     // PIN_21
#define BUTTON2_PORT         GPIOA1_BASE

// Display settings for 3D visualization
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           128
#define SCREEN_CENTER_X         (SCREEN_WIDTH / 2)
#define SCREEN_CENTER_Y         (SCREEN_HEIGHT / 2)

// 3D Rectangular Prism (servo arm) parameters
#define ARM_LENGTH              8     // Length of servo arm (X-axis)
#define ARM_HEIGHT              50    // Height of servo arm (Y-axis)
#define ARM_DEPTH               8     // Depth of servo arm (Z-axis)

#define NUM_VERTICES            16    // Two rectangular prisms = 16 vertices
#define NUM_EDGES               24    // Two rectangular prisms = 24 edges

// Application state variables
static int g_servo1Angle = 90;  // Current angle for servo 1 (0-180)
static int g_servo2Angle = 90;  // Current angle for servo 2 (0-180)
static bool g_initialized = false; // Initialization flag

// 3D Rectangular Prism vertices (x, y, z) - representing servo base and arm
float g_arm_vertices[NUM_VERTICES][3] = {
    // First rectangle (base) - Bottom vertices (Y = -ARM_HEIGHT/2)
    {-ARM_LENGTH/2, -ARM_HEIGHT/2, -ARM_DEPTH/2},   // 0: bottom back left
    {ARM_LENGTH/2, -ARM_HEIGHT/2, -ARM_DEPTH/2},    // 1: bottom back right
    {ARM_LENGTH/2, -ARM_HEIGHT/2, ARM_DEPTH/2},     // 2: bottom front right
    {-ARM_LENGTH/2, -ARM_HEIGHT/2, ARM_DEPTH/2},    // 3: bottom front left
    // First rectangle (base) - Top vertices (Y = +ARM_HEIGHT/2)
    {-ARM_LENGTH/2, ARM_HEIGHT/2, -ARM_DEPTH/2},    // 4: top back left
    {ARM_LENGTH/2, ARM_HEIGHT/2, -ARM_DEPTH/2},     // 5: top back right
    {ARM_LENGTH/2, ARM_HEIGHT/2, ARM_DEPTH/2},      // 6: top front right
    {-ARM_LENGTH/2, ARM_HEIGHT/2, ARM_DEPTH/2},     // 7: top front left

    // Second rectangle (arm) - Bottom vertices (attached to right end of first)
    {4, 25, -3},   // 8: arm bottom back left (at attachment point)
    {34, 25, -3},  // 9: arm bottom back right
    {34, 25, 3},   // 10: arm bottom front right
    {4, 25, 3},    // 11: arm bottom front left (at attachment point)
    // Second rectangle (arm) - Top vertices
    {4, 31, -3},   // 12: arm top back left (at attachment point)
    {34, 31, -3},  // 13: arm top back right
    {34, 31, 3},   // 14: arm top front right
    {4, 31, 3}     // 15: arm top front left (at attachment point)
};

// Rectangular prism edges defined as pairs of vertex indices
int g_arm_edges[NUM_EDGES][2] = {
    // First rectangle (base) - Bottom face edges
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    // First rectangle (base) - Top face edges
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    // First rectangle (base) - Vertical edges connecting bottom to top
    {0, 4}, {1, 5}, {2, 6}, {3, 7},

    // Second rectangle (arm) - Bottom face edges
    {8, 9}, {9, 10}, {10, 11}, {11, 8},
    // Second rectangle (arm) - Top face edges
    {12, 13}, {13, 14}, {14, 15}, {15, 12},
    // Second rectangle (arm) - Vertical edges connecting bottom to top
    {8, 12}, {9, 13}, {10, 14}, {11, 15}
};

// Projected 2D coordinates
int g_projected_vertices[NUM_VERTICES][2];
// Previous frame vertices for erasing
int g_prev_projected_vertices[NUM_VERTICES][2];
// Flag to track first frame
bool first_frame = true;

// Current rotation angles for visualization (based on servos)
float g_visualAngle1 = 0.0f;  // Servo 1 angle (base rotation around Y-axis)
float g_visualAngle2 = 0.0f;  // Servo 2 angle (arm rotation around X-axis)

// Forward declarations
static void ConfigTimersForServos(void);
static void SetServo1Angle(int angle);
static void SetServo2Angle(int angle);
static void ReadADCChannel(unsigned int uiChannel, float *voltageResult);
static bool ShouldExit(void);

// 3D visualization functions
static void RotatePoint(float x, float y, float z, float* rx, float* ry, float* rz, int vertexIndex);
static void ProjectPoint(float x, float y, float z, int* px, int* py);
static void RenderServoArm(uint16_t color);
static void InitializeDisplay(void);

//*****************************************************************************
// Initialize the application
//*****************************************************************************
void ServoControl_Initialize(void)
{

    // Initialize visualization angles
    g_visualAngle1 = ((float)g_servo1Angle * M_PI) / 180.0f;
    g_visualAngle2 = ((float)(g_servo2Angle - 90) * M_PI) / 180.0f;

    // Configure and enable timers for servo control
    ConfigTimersForServos();

    // Initialize display for 3D visualization
    InitializeDisplay();

    // Set initial positions
    SetServo1Angle(g_servo1Angle);
    SetServo2Angle(g_servo2Angle);

    // Wait for initialization
    MAP_UtilsDelay(8000000);

    g_initialized = true;
}

//*****************************************************************************
// Initialize display for 3D visualization
//*****************************************************************************
static void InitializeDisplay(void)
{
    // Clear the screen
    fillScreen(BLACK);
    const uint8_t* servoarm_frame_bitmap = get_servoarm_frame(0);
    fastDrawBitmap(0, 0, servoarm_frame_bitmap, SERVOARM_WIDTH, SERVOARM_HEIGHT, GREEN, BLACK, 1);

    // Initialize first frame flag
    first_frame = true;

    // Draw center point for rotation axis reference
    fillCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, 1, GREEN);
}

//*****************************************************************************
// Apply rotations to a 3D point based on which part it belongs to
//*****************************************************************************
static void RotatePoint(float x, float y, float z, float* rx, float* ry, float* rz, int vertexIndex)
{
    float temp_x = x, temp_y = y, temp_z = z;

    // If this is a vertex from the second rectangle (arm), apply servo 2 rotation first
    if (vertexIndex >= 8) {
        // The arm rotates around its long edge (Z-axis) at the attachment point
        // Translate so rotation point is at the attachment edge (Y = ARM_HEIGHT/2)
        temp_y -= ARM_HEIGHT/2;

        // Apply Z-axis rotation (servo 2) - arm bends down/up around its long edge
        float rot_x = temp_x * cosf(g_visualAngle2) - temp_y * sinf(g_visualAngle2);
        float rot_y = temp_x * sinf(g_visualAngle2) + temp_y * cosf(g_visualAngle2);

        temp_x = rot_x;
        temp_y = rot_y;

        // Translate back
        temp_y += ARM_HEIGHT/2;
    }

    // Apply Y-axis rotation (servo 1) to all vertices - base rotation
    float final_x = temp_x * cosf(g_visualAngle1/2) + temp_z * sinf(g_visualAngle1/4);
    float final_y = temp_y; // No change in Y for Y-axis rotation
    float final_z = -temp_x * sinf(g_visualAngle1/4) + temp_z * cosf(g_visualAngle1/4);

    *rx = final_x;
    *ry = final_y;
    *rz = final_z;
}

//*****************************************************************************
// Project a 3D point to 2D screen space using perspective projection
//*****************************************************************************
static void ProjectPoint(float x, float y, float z, int* px, int* py)
{
    // Perspective projection parameters
    float f = 100.0f;        // Focal length
    float z_offset = 80.0f;  // Camera distance from object

    // Apply perspective projection
    if (fabsf(z + z_offset) > 0.001f) {
        float perspective = f / (z + z_offset);
        *px = SCREEN_CENTER_X + (int)(x * perspective);
        *py = SCREEN_CENTER_Y - (int)(y * perspective);  // Y is inverted in screen space
    } else {
        // Fallback for points at camera position
        *px = SCREEN_CENTER_X + (int)x;
        *py = SCREEN_CENTER_Y - (int)y;
    }

    // Clamp to screen boundaries
    if (*px < 0) *px = 0;
    if (*px >= SCREEN_WIDTH) *px = SCREEN_WIDTH - 1;
    if (*py < 0) *py = 0;
    if (*py >= SCREEN_HEIGHT) *py = SCREEN_HEIGHT - 1;
}

//*****************************************************************************
// Render the 3D servo arm visualization
//*****************************************************************************
static void RenderServoArm(uint16_t color)
{
    int i;
    float rx, ry, rz;

    // Calculate projected vertices for current frame
    for (i = 0; i < NUM_VERTICES; i++) {
        // Apply rotation
        RotatePoint(
            g_arm_vertices[i][0],
            g_arm_vertices[i][1],
            g_arm_vertices[i][2],
            &rx, &ry, &rz, i
        );

        // Project to 2D
        ProjectPoint(rx, ry, rz,
                    &g_projected_vertices[i][0],
                    &g_projected_vertices[i][1]);
    }

    // If not the first frame, erase the previous frame by drawing black lines
    if (!first_frame) {
        // Draw black lines over previous edges
        for (i = 0; i < NUM_EDGES; i++) {
            int v1 = g_arm_edges[i][0];
            int v2 = g_arm_edges[i][1];

            drawLine(
                g_prev_projected_vertices[v1][0], g_prev_projected_vertices[v1][1],
                g_prev_projected_vertices[v2][0], g_prev_projected_vertices[v2][1],
                BLACK
            );
        }
    } else {
        first_frame = false; // No longer the first frame
    }

    // Draw new edges with the specified color
    for (i = 0; i < NUM_EDGES; i++) {
        int v1 = g_arm_edges[i][0];
        int v2 = g_arm_edges[i][1];

        drawLine(
            g_projected_vertices[v1][0], g_projected_vertices[v1][1],
            g_projected_vertices[v2][0], g_projected_vertices[v2][1],
            color
        );
    }

    // Draw a small circle at the center to show rotation axis
    drawCircle(SCREEN_CENTER_X, SCREEN_CENTER_Y, 2, color);

    // Copy current vertices to previous vertices for the next frame
    for (i = 0; i < NUM_VERTICES; i++) {
        g_prev_projected_vertices[i][0] = g_projected_vertices[i][0];
        g_prev_projected_vertices[i][1] = g_projected_vertices[i][1];
    }
}

//*****************************************************************************
// Configure timers for servo control
//*****************************************************************************
static void ConfigTimersForServos(void)
{
    PRCMPeripheralClkEnable(PRCM_GPIOA1, PRCM_RUN_MODE_CLK);
    PRCMPeripheralClkEnable(PRCM_TIMERA0, PRCM_RUN_MODE_CLK);
    PRCMPeripheralClkEnable(PRCM_TIMERA2, PRCM_RUN_MODE_CLK);
    PRCMPeripheralClkEnable(PRCM_TIMERA3, PRCM_RUN_MODE_CLK);

    //
    // Configure PIN_01 for TimerPWM6 GT_PWM06
    //
    PinTypeTimer(PIN_01, PIN_MODE_3);

    //
    // Configure PIN_02 for TimerPWM7 GT_PWM07
    //
    PinTypeTimer(PIN_02, PIN_MODE_3);

    // Calculate timer values for 300Hz frequency (3333us period)
    // Assuming 80MHz system clock
    unsigned long ulPrescaler = 0;
    unsigned long ulLoadValue = 0;

    // Calculate values for timer with 3333us period (300Hz)
    unsigned long ulClockHz = 80000000; // 80MHz system clock
    unsigned long ulPeriodCycles = (ulClockHz / SERVO_FREQ_HZ);

    // Split into prescale and load values (16-bit timer)
    ulPrescaler = ulPeriodCycles >> 16;
    ulLoadValue = ulPeriodCycles & 0xFFFF;

    // Configure TimerA3 for both A and B channels in PWM mode
    TimerConfigure(TIMERA3_BASE, (TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PWM | TIMER_CFG_B_PWM));

    // Set period (prescaler and load) for both channels
    TimerPrescaleSet(TIMERA3_BASE, TIMER_A, ulPrescaler);
    TimerLoadSet(TIMERA3_BASE, TIMER_A, ulLoadValue);

    TimerPrescaleSet(TIMERA3_BASE, TIMER_B, ulPrescaler);
    TimerLoadSet(TIMERA3_BASE, TIMER_B, ulLoadValue);

    // Calculate match value for 1.5ms pulse (center position - 90°)
    unsigned long ulMatchCycles = ulPeriodCycles - ((ulClockHz / 1000000) * SERVO_MID_US);
    unsigned long ulMatchPrescaler = ulMatchCycles >> 16;
    unsigned long ulMatchValue = ulMatchCycles & 0xFFFF;

    // Set initial match value for servo 1 (TimerA3A - PIN_01)
    TimerMatchSet(TIMERA3_BASE, TIMER_A, ulMatchValue);
    TimerPrescaleMatchSet(TIMERA3_BASE, TIMER_A, ulMatchPrescaler);

    // Set initial match value for servo 2 (TimerA3B - PIN_02)
    TimerMatchSet(TIMERA3_BASE, TIMER_B, ulMatchValue);
    TimerPrescaleMatchSet(TIMERA3_BASE, TIMER_B, ulMatchPrescaler);

    // Enable both timer channels
    TimerEnable(TIMERA3_BASE, TIMER_BOTH);
}

//*****************************************************************************
// Convert angle (0-180) to timer match value for servo 1 (TimerA3A - PIN_01)
//*****************************************************************************
static void SetServo1Angle(int angle)
{
    unsigned long ulClockHz = 80000000;
    unsigned long ulPeriodCycles = (ulClockHz / SERVO_FREQ_HZ);
    unsigned long ulPulseWidthUs;

    // Ensure angle is in valid range
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // Map angle to pulse width (1000-2000us)
    ulPulseWidthUs = SERVO_MIN_US + ((SERVO_MAX_US - SERVO_MIN_US) * (180 - angle) / 180);

    // Calculate match value for desired pulse width
    unsigned long ulMatchCycles = ulPeriodCycles - ((ulClockHz / 1000000) * ulPulseWidthUs);
    unsigned long ulMatchPrescaler = ulMatchCycles >> 16;
    unsigned long ulMatchValue = ulMatchCycles & 0xFFFF;

    // Set match value for TimerA3A
    TimerMatchSet(TIMERA3_BASE, TIMER_A, ulMatchValue);
    TimerPrescaleMatchSet(TIMERA3_BASE, TIMER_A, ulMatchPrescaler);

    // Update visualization angle (convert servo angle to radians)
    // Maps servo angle directly to visual rotation around Y-axis
    g_visualAngle1 = (180+1*(float)(angle) * M_PI) / 180.0f;

}

//*****************************************************************************
// Convert angle (0-180) to timer match value for servo 2 (TimerA3B - PIN_02)
//*****************************************************************************
static void SetServo2Angle(int angle)
{
    unsigned long ulClockHz = 80000000;
    unsigned long ulPeriodCycles = (ulClockHz / SERVO_FREQ_HZ);
    unsigned long ulPulseWidthUs;

    // Ensure angle is in valid range
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // Map angle to pulse width (1000-2000us)
    ulPulseWidthUs = SERVO_MIN_US + ((SERVO_MAX_US - SERVO_MIN_US) * angle / 180);

    // Calculate match value for desired pulse width
    unsigned long ulMatchCycles = ulPeriodCycles - ((ulClockHz / 1000000) * ulPulseWidthUs);
    unsigned long ulMatchPrescaler = ulMatchCycles >> 16;
    unsigned long ulMatchValue = ulMatchCycles & 0xFFFF;

    // Set match value for TimerA3B
    TimerMatchSet(TIMERA3_BASE, TIMER_B, ulMatchValue);
    TimerPrescaleMatchSet(TIMERA3_BASE, TIMER_B, ulMatchPrescaler);

    // Update visualization angle for second servo (convert servo angle to radians)
    // Maps servo 2 angle to arm rotation around X-axis (subtract 90 to center at 0)
    g_visualAngle2 = (-1*(float)(angle - 90) * M_PI) / 180.0f;
}

//*****************************************************************************
// Read ADC Channel and calculate voltage
//*****************************************************************************
static void ReadADCChannel(unsigned int uiChannel, float *voltageResult)
{
    unsigned int uiIndex = 0;
    unsigned long ulSample;
    float avgVoltage = 0.0;

    // Enable ADC channel
    MAP_ADCChannelEnable(ADC_BASE, uiChannel);

    // Sample 10 times and average
    uiIndex = 0;
    while(uiIndex < 5)
    {
        if(MAP_ADCFIFOLvlGet(ADC_BASE, uiChannel))
        {
            ulSample = MAP_ADCFIFORead(ADC_BASE, uiChannel);
            // Calculate voltage: (ADC value * 1.4V reference) / 4096 (12-bit resolution)
            avgVoltage += (((float)((ulSample >> 2) & 0x0FFF)) * 1.4) / 4096;
            uiIndex++;
        }
    }

    // Disable ADC channel
    MAP_ADCChannelDisable(ADC_BASE, uiChannel);

    // Calculate average voltage
    *voltageResult = avgVoltage / 5.0;
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
// Run one frame of the application
//*****************************************************************************
bool ServoControl_RunFrame(void)
{

    char display_angle1[3];
    char display_angle2[3];

    if (!g_initialized) {
        ServoControl_Initialize();
        return true;
    }

    // Check if button 2 is pressed to exit
    if (ShouldExit()) {
        return false;
    }

    // Read joystick inputs
    float voltage_x, voltage_y;
    ReadADCChannel(ADC_CH_2, &voltage_x);  // X-axis
    ReadADCChannel(ADC_CH_3, &voltage_y);  // Y-axis

    // Control servos based on joystick readings
    int newServo1Angle = g_servo1Angle;
    int newServo2Angle = g_servo2Angle;

    // X-axis controls servo 1
    if (fabs(((voltage_x)/1.4) - 0.5) >= 0.1) {
        // Map joystick X to servo 1 angle
        float joystickX = ((voltage_x)/1.4) - 0.45;  // -0.5 to 0.5
        newServo1Angle = newServo1Angle + (10*joystickX);  // Map to 0-180 range

        // Ensure angle is in valid range
        if (newServo1Angle < 0) newServo1Angle = 0;
        if (newServo1Angle > 180) newServo1Angle = 180;
    }

    // Y-axis controls servo 2
    if (((voltage_y)/1.4) - 0.5 >= 0.1) {
        // Map joystick Y to servo 2 angle
        float joystickY = ((voltage_y)/1.4) - 0.5;  // -0.5 to 0.5
        newServo2Angle = newServo2Angle + (14*joystickY);  // Map to 0-180 range

        // Ensure angle is in valid range
        if (newServo2Angle < 0) newServo2Angle = 0;
        if (newServo2Angle > 180) newServo2Angle = 180;
    }
    else if (((voltage_y)/1.4) - 0.5 <= -0.1) {
        // Map joystick Y to servo 2 angle
        float joystickY = ((voltage_y)/1.4) - 0.5;  // -0.5 to 0.5
        newServo2Angle = newServo2Angle + (8*joystickY);  // Map to 0-180 range

        // Ensure angle is in valid range
        if (newServo2Angle < 0) newServo2Angle = 0;
        if (newServo2Angle > 180) newServo2Angle = 180;
    }

    // Update servos if angles have changed
    if (newServo1Angle != g_servo1Angle) {
        g_servo1Angle = newServo1Angle;
        SetServo1Angle(g_servo1Angle);
    }

    if (newServo2Angle != g_servo2Angle) {
        g_servo2Angle = newServo2Angle;
        SetServo2Angle(g_servo2Angle);
    }

    // Print the angles on screen
    sprintf(display_angle1, "%d", g_servo1Angle/2);
    sprintf(display_angle2,"%d", g_servo2Angle);
    Outstr(display_angle1, GREEN, BLACK, 30, 117, 128, 125);
    Outstr(display_angle2, GREEN, BLACK, 90, 117, 128, 125);

    // Render the 3D servo arm visualization
    RenderServoArm(GREEN);

    // Small delay between frames
    MAP_UtilsDelay(40000);

    //Clear text
    fillRect(28, 115, 35, 11, BLACK);
    fillRect(88, 115, 35, 11, BLACK);
    return true;
}

//*****************************************************************************
// Clean up resources before exiting
//*****************************************************************************
void ServoControl_Cleanup(void)
{
    // Center servos before exiting
    SetServo1Angle(90);
    SetServo2Angle(90);

    // Clear display
    fillScreen(BLACK);

    PinMuxConfig();

    g_initialized = false;
}
