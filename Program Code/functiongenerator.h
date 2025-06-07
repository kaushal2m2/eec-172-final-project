//*****************************************************************************
// Function Generator Header with Integrated Visualizer
// Square wave generation with real-time display
//*****************************************************************************

#ifndef FUNCTIONGENERATOR_H_
#define FUNCTIONGENERATOR_H_

#include <stdbool.h>

//*****************************************************************************
// Function Declarations
//*****************************************************************************

//*****************************************************************************
// Initialize the Function Generator with integrated visualizer
// Sets up both audio generation and display components
//*****************************************************************************
void FunctionGenerator_Initialize(void);

//*****************************************************************************
// Set Function Generator Frequency
// Parameters:
//   frequency - Output frequency in Hz
//*****************************************************************************
void FunctionGenerator_SetFrequency(unsigned long frequency);

//*****************************************************************************
// Enable/Disable Function Generator Output
// Parameters:
//   enable - true to enable output, false to disable
//*****************************************************************************
void FunctionGenerator_Enable(bool enable);

//*****************************************************************************
// Run one frame of the function generator with visualization
// Handles frequency sweeping, display updates, and user input
// Returns: true to continue running, false to exit (button 2 pressed)
//*****************************************************************************
bool FunctionGenerator_RunFrame(void);

//*****************************************************************************
// Clean up function generator resources
// Stops output and clears display
//*****************************************************************************
void FunctionGenerator_Cleanup(void);

//*****************************************************************************
// Get current function generator status
// Returns: true if output is enabled, false if disabled
//*****************************************************************************
bool FunctionGenerator_IsEnabled(void);

//*****************************************************************************
// Get current frequency setting
// Returns: Current frequency in Hz
//*****************************************************************************
unsigned long FunctionGenerator_GetFrequency(void);

//*****************************************************************************
// Quick frequency change function - immediate effect
// Parameters:
//   frequency - New frequency in Hz (0 to stop)
//*****************************************************************************
void FunctionGenerator_PlayFrequency();

//*****************************************************************************
// Stop output immediately
// Sets frequency to 0 and disables output
//*****************************************************************************
void FunctionGenerator_Stop(void);

#endif /* FUNCTIONGENERATOR_H_ */
