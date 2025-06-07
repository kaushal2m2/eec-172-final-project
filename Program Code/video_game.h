//*****************************************************************************
// Simple Video Game with Physics
// Circle that can jump and move with analog stick control
//*****************************************************************************

#ifndef VIDEO_GAME_H
#define VIDEO_GAME_H

// Initialize the video game
void VideoGame_Initialize(void);

// Run one frame of the video game
// Returns: true to continue running, false to exit
bool VideoGame_RunFrame(void);

// Clean up resources before exiting
void VideoGame_Cleanup(void);

#endif // VIDEO_GAME_H
