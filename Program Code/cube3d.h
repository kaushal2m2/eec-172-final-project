#ifndef CUBE3D_H
#define CUBE3D_H
#include <stdbool.h>

// Function to initialize the 3D cube application
void Cube3D_Initialize(void);

// Function to run one frame of the 3D cube application
// Returns: true to continue running, false to exit
bool Cube3D_RunFrame(void);

// Function to check if button 2 is pressed to exit
bool Cube3D_ShouldExit(void);

// Function to clean up any resources before exiting
void Cube3D_Cleanup(void);

#endif // CUBE3D_H
