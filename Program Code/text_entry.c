//*****************************************************************************
//
// text_entry.c - Enhanced Text Entry Interface Implementation
//
// Provides on-screen keyboard functionality using joystick navigation
// and button selection for entering text questions.
// Enhanced with 2D navigation, backspace, enter key callback support,
// and special question type functionality.
//
//*****************************************************************************

#include "text_entry.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

// Include necessary headers from main program
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "shared_defs.h"
#include "uart_if.h"
#include "adc.h"
#include "hw_adc.h"
#include "rom_map.h"
#include "gpio.h"
#include "electronichelper_bitmap.h"

//*****************************************************************************
// Constants and Definitions
//*****************************************************************************

// Enhanced keyboard layout - 2 rows with backspace
#define KEYBOARD_ROWS       6
#define KEYBOARD_COLS       10
#define CHAR_SPACING_X      11
#define CHAR_SPACING_Y      11
#define KEYBOARD_START_X    11
#define KEYBOARD_START_Y    61
#define CHAR_SIZE           1
#define HIGHLIGHT_SIZE      11

// Button pins (from main program)
#define BUTTON1_PIN         0x40    // PIN_15
#define BUTTON1_PORT        GPIOA2_BASE
#define BUTTON2_PIN         0x20    // PIN_21
#define BUTTON2_PORT        GPIOA1_BASE

// Display areas
#define QUESTION_AREA_Y     10
#define QUESTION_MAX_CHARS  45
#define KEYBOARD_AREA_HEIGHT 60

// Colors
#define TEXT_COLOR          WHITE
#define HIGHLIGHT_COLOR     BLUE
#define BACKGROUND_COLOR    BLACK
#define QUESTION_COLOR      YELLOW
#define BACKSPACE_COLOR     RED
#define ENTER_COLOR         GREEN
#define TOGGLE_COLOR        MAGENTA  // Color for toggled question type buttons

// Special character index for backspace
#define BACKSPACE_CHAR_INDEX 4  // Position in first row

//*****************************************************************************
// Global Variables
//*****************************************************************************

// Enhanced keyboard characters - 2D array
// # represents enter
// ? represents pin labels, ! represents pin connections, * represents component purpose
static const char keyboard_chars[KEYBOARD_ROWS][KEYBOARD_COLS] = {
    {'?', '?', '?', '?', '!', '!', '!', '*', '*', '*'},
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'},
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '#'},
    {'z', 'x', 'c', 'v', 'b', 'n', 'm', '+', '-', '#'},
    {' ',' ',' ',' ',' ',' ',' ','<','<', '<'}
};

// Current selection position
static int current_col = 0;
static int current_row = 0;
static int previous_col = -1;
static int previous_row = -1;

// State variables
static bool initialized = false;
static bool button1_was_pressed = false;
static bool button2_was_pressed = false;

// Enhanced joystick state for 2D navigation
static bool joystick_moved_right = false;
static bool joystick_moved_left = false;
static bool joystick_moved_up = false;
static bool joystick_moved_down = false;
static float last_joystick_x = 0.5f;
static float last_joystick_y = 0.5f;
static bool enter_pressed = false;

// Internal text buffer for the question being built
static char current_question[256] = {0};

// Callback function for enter key
static TextEntryEnterCallback g_enter_callback = NULL;

// Flag to request exit after enter key processing
static bool g_exit_requested = false;

// Current question type for special functionality
static char current_question_type = 0; // 0 = none, '?' = pin labels, '!' = pin connect, '*' = comp purpose

static bool ToggleClear = false;

//*****************************************************************************
// Function Prototypes
//*****************************************************************************
static void ReadADCChannel(unsigned int uiChannel, float *voltageResult);
static void DisplayKeyboard(void);
static void DisplayCurrentQuestion(void);
static void ProcessJoystickInput(void);
static void ProcessButtonInput(void);
static bool IsButton1Pressed(void);
static bool IsButton2Pressed(void);
static void ClearPreviousHighlight(void);
static void DrawCharacterAt(int row, int col, bool highlight, bool eraseHighlight);
static bool IsQuestionTypeToggled(char character);
static const char* GetQuestionTypePrefix(void);

//*****************************************************************************
// Check if a question type character is currently toggled
//*****************************************************************************
static bool IsQuestionTypeToggled(char character)
{
    return (current_question_type == character && !ToggleClear);
}

//*****************************************************************************
// Get the prefix string for the current question type
//*****************************************************************************
static const char* GetQuestionTypePrefix(void)
{
    switch(current_question_type) {
        case '?':
            return "pin labels/";
        case '!':
            return "pin connect/";
        case '*':
            return "comp purpose/";
        default:
            return "";
    }
}

//*****************************************************************************
// ADC Reading Function (from main program)
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
    while(uiIndex < 10)
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
    *voltageResult = avgVoltage / 10.0;
}

//*****************************************************************************
// Button State Reading Functions
//*****************************************************************************
static bool IsButton1Pressed(void)
{
    return !(GPIOPinRead(BUTTON1_PORT, BUTTON1_PIN) == 0);
}

static bool IsButton2Pressed(void)
{
    return !(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0);
}

//*****************************************************************************
// Clear the highlight from the previous selection
//*****************************************************************************
static void ClearPreviousHighlight(void)
{
    if (previous_row >= 0 && previous_col >= 0) {
        DrawCharacterAt(previous_row, previous_col, false, true);
    }
}

//*****************************************************************************
// Draw a character at specified row/col with optional highlight
//*****************************************************************************
static void DrawCharacterAt(int row, int col, bool highlight, bool eraseHighlight)
{
    if (row < 0 || row >= KEYBOARD_ROWS || col < 0 || col >= KEYBOARD_COLS) {
        return;
    }

    int char_x = KEYBOARD_START_X + (col * CHAR_SPACING_X);
    int char_y = KEYBOARD_START_Y + (row * CHAR_SPACING_Y);
    char character = keyboard_chars[row][col];

    // Check if this character should be persistently highlighted (toggled)
    bool is_toggled = IsQuestionTypeToggled(character);

    // Define which positions are the "primary" positions for grouped characters
    // to avoid multiple draws/erases to the same rectangle
    bool should_process_grouped_char = true;
    bool is_grouped_char = false;
    bool cursor_in_group = false;

    // Check for grouped characters and determine primary position
    if (character == '?') {
        is_grouped_char = true;
        should_process_grouped_char = (col == 1); // Only process '?' at column 1
        cursor_in_group = (current_row == 0 && current_col >= 0 && current_col <= 3);
    } else if (character == '!') {
        is_grouped_char = true;
        should_process_grouped_char = (col == 5); // Only process '!' at column 5
        cursor_in_group = (current_row == 0 && current_col >= 4 && current_col <= 6);
    } else if (character == '*') {
        is_grouped_char = true;
        should_process_grouped_char = (col == 8); // Only process '*' at column 8
        cursor_in_group = (current_row == 0 && current_col >= 7 && current_col <= 9);
    } else if (character == '#') {
        is_grouped_char = true;
        should_process_grouped_char = (row == 3 && col == 9); // Only process '#' at (3,9)
        cursor_in_group = ((current_row == 3 && current_col == 9) || (current_row == 4 && current_col == 9));
    } else if (character == '<') {
        is_grouped_char = true;
        should_process_grouped_char = (col == 8); // Only process '<' at column 8
        cursor_in_group = (current_row == 5 && current_col >= 7 && current_col <= 9);
    } else if (character == ' ') {
        is_grouped_char = true;
        should_process_grouped_char = (col == 3); // Only process ' ' at column 3
        cursor_in_group = (current_row == 5 && current_col >= 0 && current_col <= 6);
    } else {
        // Non-grouped character - use normal logic
        is_grouped_char = false;
        should_process_grouped_char = true;
        cursor_in_group = highlight;
    }

    // Skip processing for non-primary positions of grouped characters
    if (is_grouped_char && !should_process_grouped_char) {
        return;
    }

    // Draw highlight rectangle if selected OR toggled
    if (cursor_in_group || is_toggled) {
        // Determine highlight color - prioritize selection over toggle
        int highlight_color = HIGHLIGHT_COLOR;
        if (cursor_in_group) {
            // If currently selected, use appropriate selection color
            if (character == '#') {
                highlight_color = ENTER_COLOR;
            } else if (character == '<') {
                highlight_color = BACKSPACE_COLOR;
            } else {
                highlight_color = HIGHLIGHT_COLOR; // Blue for normal selection
            }
        } else if (is_toggled) {
            // Only use toggle color if not currently selected
            highlight_color = TOGGLE_COLOR; // Magenta for toggled question types
        }

        // special shaped character processing
        switch(character) {
            case '#':
                drawRect(109, 93, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE*2, highlight_color);
                break;
            case ' ':
                drawRect(42, 115, (HIGHLIGHT_SIZE*4)+1, HIGHLIGHT_SIZE, highlight_color);
                break;
            case '<':
                drawRect(87, 115, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE, highlight_color);
                break;
            case '?':
                drawRect(13, 19, 33, 17, highlight_color);
                break;
            case '!':
                drawRect(46, 19, 35, 17, highlight_color);
                break;
            case '*':
                drawRect(81, 19, 35, 17, highlight_color);
                break;
            default:
                // Normal Character highlighting
                drawRect(char_x-1, char_y-1, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE, highlight_color);
                break;
        }
    } else if ((eraseHighlight && !is_toggled)||(ToggleClear)) { // Only erase if not toggled, or special toggleclear instruction
        switch(character) {
            case '#':
                drawRect(109, 93, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE*2, BLACK);
                break;
            case ' ':
                drawRect(42, 115, (HIGHLIGHT_SIZE*4)+1, HIGHLIGHT_SIZE, BLACK);
                break;
            case '<':
                drawRect(87, 115, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE, BLACK);
                break;
            case '?':
                drawRect(13, 19, 33, 17, BLACK);
                break;
            case '!':
                drawRect(46, 19, 35, 17, BLACK);
                break;
            case '*':
                drawRect(81, 19, 35, 17, BLACK);
                break;
            default:
                drawRect(char_x-1, char_y-1, HIGHLIGHT_SIZE, HIGHLIGHT_SIZE, BLACK);
                break;
        }
    }
}

//*****************************************************************************
// Display the enhanced on-screen keyboard
//*****************************************************************************
static void DisplayKeyboard(void)
{
    int row, col;

    // Draw all characters
    for (row = 0; row < KEYBOARD_ROWS; row++) {
        for (col = 0; col < KEYBOARD_COLS; col++) {
            bool is_selected = (row == current_row && col == current_col);
            // Always pass true for eraseHighlight to ensure proper clearing of untoggled items
            DrawCharacterAt(row, col, is_selected, true);
        }
    }

    setTextSize(1);

    // Update previous selection tracking
    previous_row = current_row;
    previous_col = current_col;
}

//*****************************************************************************
// Display the current question being built
//*****************************************************************************
static void DisplayCurrentQuestion(void)
{
    char display_question[QUESTION_MAX_CHARS + 1];
    int question_len = strlen(current_question);

    if (question_len <= QUESTION_MAX_CHARS) {
        strcpy(display_question, current_question);
    } else {
        // Show last QUESTION_MAX_CHARS characters
        strncpy(display_question,
                current_question + (question_len - QUESTION_MAX_CHARS),
                QUESTION_MAX_CHARS);
        display_question[QUESTION_MAX_CHARS] = '\0';
    }
    Outstr(display_question, GREEN, BLACK, 14, 40, 118, 69);

    char count_str[20];
    sprintf(count_str, "Length: %d", question_len);
    Outstr(count_str, GREEN, BLACK, 14, 61, 118, 69);

}

//*****************************************************************************
// Process enhanced joystick input for 2D character selection
//*****************************************************************************
static void ProcessJoystickInput(void)
{
    float voltage_x, voltage_y;

    // Read joystick X and Y positions
    ReadADCChannel(ADC_CH_2, &voltage_x);  // X-axis (left/right)
    ReadADCChannel(ADC_CH_3, &voltage_y);  // Y-axis (up/down) - may need adjustment based on hardware

    // Convert to normalized positions (0.0 to 1.0)
    float joystick_x = voltage_x / 1.4;
    float joystick_y = voltage_y / 1.4;

    // Detect joystick movement with deadzone
    float deadzone = 0.1f;
    bool moved_right = false;
    bool moved_left = false;
    bool moved_up = false;
    bool moved_down = false;

    // Horizontal movement detection
    if (joystick_x > (0.5 + deadzone) && last_joystick_x <= (0.5 + deadzone)) {
        moved_left = true;
    } else if (joystick_x < (0.5 - deadzone) && last_joystick_x >= (0.5 - deadzone)) {
        moved_right = true;
    }

    // Vertical movement detection
    if (joystick_y > (0.5 + deadzone) && last_joystick_y <= (0.5 + deadzone)) {
        moved_down = true;
    } else if (joystick_y < (0.5 - deadzone) && last_joystick_y >= (0.5 - deadzone)) {
        moved_up = true;
    }

    // Clear previous highlight before moving
    bool selection_changed = false;

    // Update column selection based on horizontal movement
    if (moved_right && !joystick_moved_right) {
        ClearPreviousHighlight();
        // special instruction for spacebar moving to backspace
        if(keyboard_chars[current_row][current_col] == ' '){
            current_col = 9;
        }
        else if(keyboard_chars[current_row][current_col] == '?'){
            current_col = 4;
        }
        else if(keyboard_chars[current_row][current_col] == '!'){
            current_col = 9;
        }
        else{
        current_col++;
        }

        if (current_col >= KEYBOARD_COLS) {
            current_col = KEYBOARD_COLS - 1; // Stay at rightmost
        }
        joystick_moved_right = true;
        selection_changed = true;
        Report("Moved right to col %d\n\r", current_col);
        }
     else if (moved_left && !joystick_moved_left) {
        ClearPreviousHighlight();
        if(keyboard_chars[current_row][current_col] == '<'){
             current_col = 6;
        }
        else if(keyboard_chars[current_row][current_col] == '!'){
              current_col = 1;
        }
        else if(keyboard_chars[current_row][current_col] == '*'){
                      current_col = 5;
        }
        else{
        current_col--;
        }
        if (current_col < 0) {
            current_col = 0; // Stay at leftmost
        }
        joystick_moved_left = true;
        selection_changed = true;
        Report("Moved left to col %d\n\r", current_col);
    }

    // Update row selection based on vertical movement
    if (moved_down && !joystick_moved_down) {
        ClearPreviousHighlight();
        current_row++;
        if (current_row >= KEYBOARD_ROWS) {
            current_row = KEYBOARD_ROWS - 1; // Stay at bottom
        }
        joystick_moved_down = true;
        selection_changed = true;
        Report("Moved down to row %d\n\r", current_row);
    } else if (moved_up && !joystick_moved_up) {
        ClearPreviousHighlight();
        if(keyboard_chars[current_row][current_col] == ' '){
            current_col = 4;
        }
        else if(keyboard_chars[current_row][current_col] == '<'){
            current_col = 7;
        }
        current_row--;
        if (current_row < 0) {
            current_row = 0; // Stay at top
        }
        joystick_moved_up = true;
        selection_changed = true;
        Report("Moved up to row %d\n\r", current_row);
    }

    // Reset movement flags when joystick returns to center
    if (fabs(joystick_x - 0.5) < deadzone) {
        joystick_moved_right = false;
        joystick_moved_left = false;
    }
    if (fabs(joystick_y - 0.5) < deadzone) {
        joystick_moved_up = false;
        joystick_moved_down = false;
    }

    last_joystick_x = joystick_x;
    last_joystick_y = joystick_y;
}

//*****************************************************************************
// Process button input for character selection and exit
//*****************************************************************************
static void ProcessButtonInput(void)
{
    bool button1_pressed = IsButton1Pressed();
    bool button2_pressed = IsButton2Pressed();

    if(button2_pressed && !button2_was_pressed){
        ToggleClear = false; // Reset toggle clear
    }

    // Handle Button 1 - Select character or backspace
    if (button1_pressed && !button1_was_pressed) {
        char selected_char = keyboard_chars[current_row][current_col];
        fillRect(13, 61, 67, 7,BLACK);
        if (selected_char == '<') {
            // Handle backspace
            int question_len = strlen(current_question);
            if (question_len > 0) {
                current_question[question_len - 1] = '\0';
                fillRect(14, 39, 104, 23,BLACK);
                Report("Backspace - removed character, new string: '%s'\n\r", current_question);
            } else {
                Report("Backspace - string already empty\n\r");
            }
        } else if (selected_char == '#') {
            // Handle ENTER key - trigger callback with prefixed question
            if (g_enter_callback != NULL) {
                // Create the final question with prefix
                char final_question[384]; // Larger buffer for prefix + question
                const char* prefix = GetQuestionTypePrefix();
                snprintf(final_question, sizeof(final_question), "%s%s", prefix, current_question);

                Report("Enter pressed - calling callback with question: '%s'\n\r", final_question);
                g_enter_callback(final_question);
            } else {
                Report("Enter pressed but no callback registered\n\r");
            }
        } else if (selected_char == '?' || selected_char == '!' || selected_char == '*') {
            // Handle question type toggle
            if (current_question_type == selected_char) {
                // If already selected, deselect
                current_question_type = 0;
                Report("Question type '%c' deselected\n\r", selected_char);
            } else {
                // Select this question type
                current_question_type = selected_char;
                Report("Question type '%c' selected: %s\n\r", selected_char, GetQuestionTypePrefix());
            }
            // Redraw the entire keyboard to update toggle states
            DisplayKeyboard();
        } else if (selected_char == ' ') {
            // Handle space character
            int question_len = strlen(current_question);
            if (question_len < sizeof(current_question) - 1) {
                current_question[question_len] = ' ';
                current_question[question_len + 1] = '\0';
                Report("Added space to question: '%s'\n\r", current_question);
            }
        } else {
            // Handle regular character
            int question_len = strlen(current_question);
            if (question_len < sizeof(current_question) - 1) {
                current_question[question_len] = selected_char;
                current_question[question_len + 1] = '\0';

                Report("Added character '%c' to question: '%s'\n\r",
                           selected_char, current_question);
            } else {
                Report("Question string is full!\n\r");
            }
        }
    }

    button1_was_pressed = button1_pressed;
    button2_was_pressed = button2_pressed;
}

//*****************************************************************************
// Initialize the enhanced text entry interface
//*****************************************************************************
void TextEntry_Initialize(const char* initial_text, TextEntryEnterCallback enter_callback)
{
    Report("Initializing enhanced text entry interface...\n\r");

    // Store the enter callback function
    g_enter_callback = enter_callback;

    // Reset exit request flag
    g_exit_requested = false;

    // Reset question type
    current_question_type = 0;

    // Clear the screen
    fillScreen(BACKGROUND_COLOR);

    // Initialize the question string
    if (initial_text != NULL && strlen(initial_text) < sizeof(current_question)) {
        strcpy(current_question, initial_text);
    } else {
        current_question[0] = '\0'; // Empty string
    }

    // Initialize state
    current_col = 0;
    current_row = 0;
    previous_col = -1;
    previous_row = -1;

    button1_was_pressed = false;
    button2_was_pressed = false;
    joystick_moved_right = false;
    joystick_moved_left = false;
    joystick_moved_up = false;
    joystick_moved_down = false;
    last_joystick_x = 0.5f;
    last_joystick_y = 0.5f;
    ToggleClear = false;

    // Display initial interface
    DisplayCurrentQuestion();
    DisplayKeyboard();

    initialized = true;

    const uint8_t* electronichelper_frame_bitmap = get_electronichelper_frame(0);
    fastDrawBitmap(0, 0, electronichelper_frame_bitmap, ELECTRONICHELPER_WIDTH, ELECTRONICHELPER_HEIGHT, GREEN, BLACK, 1);

    Report("Enhanced text entry interface initialized with text: '%s'\n\r", current_question);
    Report("Current position: row %d, col %d\n\r", current_row, current_col);
    Report("Enter callback %s\n\r", (enter_callback != NULL) ? "registered" : "not registered");
}

//*****************************************************************************
// Run one frame of the enhanced text entry interface
//*****************************************************************************
bool TextEntry_RunFrame(void)
{
    if (!initialized) {
        return false;
    }

    // Check for exit request from callback
    if (g_exit_requested) {
        Report("Exit requested - exiting text entry\n\r");
        return false; // Exit text entry
    }

    // Check for exit condition (Button 2)
    if (IsButton2Pressed() && !button2_was_pressed) {
        Report("Button 2 pressed - exiting text entry\n\r");
        return false; // Exit text entry
    }
    button2_was_pressed = IsButton2Pressed();

    // Process input
    ProcessJoystickInput();
    ProcessButtonInput();

    // Update display
    DisplayCurrentQuestion();
    DisplayKeyboard();

    return true; // Continue text entry
}

//*****************************************************************************
// Get the current text string
//*****************************************************************************
const char* TextEntry_GetCurrentText(void)
{
    return current_question;
}

//*****************************************************************************
// Request exit from text entry interface
//*****************************************************************************
void TextEntry_RequestExit(void)
{
    Report("Text entry exit requested\n\r");
    g_exit_requested = true;
}

void Clear_toggle(void){
    ToggleClear = true;
}

//*****************************************************************************
// Clean up the enhanced text entry interface
//*****************************************************************************
void TextEntry_Cleanup(void)
{
    Report("Cleaning up enhanced text entry interface...\n\r");


    // Clear the callback
    g_enter_callback = NULL;

    // Reset state
    initialized = false;

    Report("Enhanced text entry cleanup complete\n\r");
    Report("Final question: '%s'\n\r", current_question);
}
