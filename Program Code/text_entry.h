//*****************************************************************************
//
// text_entry.h - Text Entry Interface for AWS IoT Question Input
//
// Provides on-screen keyboard functionality using joystick navigation
// and button selection for entering text questions.
// Enhanced with callback support for enter key functionality.
//
//*****************************************************************************
#ifndef TEXT_ENTRY_H_
#define TEXT_ENTRY_H_
#include <stdbool.h>

//*****************************************************************************
// Function Pointer Types
//*****************************************************************************

//*****************************************************************************
//
//! Function pointer type for enter key callback
//!
//! This callback will be called when the user presses the '#' (enter) key
//! in the text entry interface.
//!
//! \param text - The current text string when enter was pressed
//!
//! \return None
//
//*****************************************************************************
typedef void (*TextEntryEnterCallback)(const char* text);

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

//*****************************************************************************
//
//! Initialize the text entry interface
//!
//! Sets up the on-screen keyboard and displays the current question text.
//! Should be called once when entering text entry mode.
//!
//! \param initial_text - Initial text to display (can be NULL for empty)
//! \param enter_callback - Function to call when enter ('#') key is pressed
//!                         (can be NULL if no callback needed)
//!
//! \return None
//
//*****************************************************************************
void TextEntry_Initialize(const char* initial_text, TextEntryEnterCallback enter_callback);

//*****************************************************************************
//
//! Run one frame of the text entry interface
//!
//! Processes joystick input for navigation, button input for selection,
//! and updates the display. Should be called repeatedly in a loop.
//!
//! \return true to continue text entry, false to exit
//
//*****************************************************************************
bool TextEntry_RunFrame(void);

//*****************************************************************************
//
//! Get the current text string
//!
//! Returns the current text that has been entered.
//!
//! \return Pointer to the current text string
//
//*****************************************************************************
const char* TextEntry_GetCurrentText(void);

//*****************************************************************************
//
//! Clean up the text entry interface
//!
//! Clears the screen and performs any necessary cleanup when exiting
//! text entry mode.
//!
//! \return None
//
//*****************************************************************************
void TextEntry_Cleanup(void);

//*****************************************************************************
//
//! Request exit from text entry interface
//!
//! This function can be called by callback functions to request that
//! the text entry interface exit on the next frame.
//!
//! \return None
//
//*****************************************************************************
void TextEntry_RequestExit(void);

void Clear_toggle(void);

//*****************************************************************************
//
//! Get rid of the toggled highlights when transitioning screens
//
//*****************************************************************************


#endif /* TEXT_ENTRY_H_ */
