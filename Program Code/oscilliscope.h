/*
 * oscilliscope.h
 *
 *  Created on: May 19, 2025
 *      Author: lfiel
 */

#ifndef OSCILLISCOPE_H_
#define OSCILLISCOPE_H_

// Initialize the servo control application
void Oscilloscope_Initialize(void);

// Run one frame of the servo control application
// Returns true to continue, false to exit
bool Oscilloscope_RunFrame(void);

// Clean up resources before exiting
void Oscilloscope_Cleanup(void);

#endif /* OSCILLISCOPE_H_ */
