//*****************************************************************************
// Servo Control Application
// Controls two SF006C digital servos using joystick input on CC3200
//*****************************************************************************

#ifndef __SERVO_CONTROL_H__
#define __SERVO_CONTROL_H__

// Initialize the servo control application
void ServoControl_Initialize(void);

// Run one frame of the servo control application
// Returns true to continue, false to exit
bool ServoControl_RunFrame(void);

// Clean up resources before exiting
void ServoControl_Cleanup(void);

#endif // __SERVO_CONTROL_H__
