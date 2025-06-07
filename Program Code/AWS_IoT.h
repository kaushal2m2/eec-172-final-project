//*****************************************************************************
//
// AWS_IoT.h - AWS IoT Integration Module for CC3200 Main Menu
//
//*****************************************************************************

#ifndef __AWS_IOT_H__
#define __AWS_IOT_H__

//*****************************************************************************
// Initialize AWS IoT Application
// - Connects to Wi-Fi
// - Sets device time
// - Establishes TLS connection to AWS IoT
// - Sends initial question
//*****************************************************************************
void AWSIoT_Initialize(void);

//*****************************************************************************
// Run one frame of the AWS IoT application
// - Checks for shadow updates
// - Updates the display
// - Monitors exit button
//
// Returns: true if continuing, false if should exit
//*****************************************************************************
bool AWSIoT_RunFrame(void);

//*****************************************************************************
// Clean up AWS IoT application resources
// - Closes TLS connection
// - Resets state variables
// - Clears display
//*****************************************************************************
void AWSIoT_Cleanup(void);

#endif // __AWS_IOT_H__
