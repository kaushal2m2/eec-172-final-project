#include "simplelink.h"
#include <string.h>


// Monochrome bitmap animation data for character_run_left.gif
// Size: 13x17 pixels, 4 frames
#define CHARACTER_RUN_LEFT_WIDTH 13
#define CHARACTER_RUN_LEFT_HEIGHT 17
#define CHARACTER_RUN_LEFT_FRAME_COUNT 4
#define CHARACTER_RUN_LEFT_FRAME_SIZE 34

// Function to get a pointer to a specific frame
const uint8_t* get_character_run_left_frame(uint16_t frame_index) {
    static uint8_t frameBuffer[CHARACTER_RUN_LEFT_FRAME_SIZE];

    // Ensure valid frame index
    if (frame_index >= CHARACTER_RUN_LEFT_FRAME_COUNT) {
        frame_index = 0;
    }

    // Format filename
    char filename[64];
    sprintf(filename, "/character_run_leftFrames_%d.bin", frame_index);

    // Try opening the file - cast to unsigned char*
    long fileHandle;
    int status = sl_FsOpen((unsigned char*)filename, FS_MODE_OPEN_READ, NULL, &fileHandle);

    if (status < 0) {
        // Default pattern
        memset(frameBuffer, 0, CHARACTER_RUN_LEFT_FRAME_SIZE);
        frameBuffer[3] = 0x08;  // Default dot
        return frameBuffer;
    }

    // Read data
    long bytesRead = sl_FsRead(fileHandle, 0, frameBuffer, CHARACTER_RUN_LEFT_FRAME_SIZE);
    sl_FsClose(fileHandle, 0, 0, 0);

    return frameBuffer;
}
