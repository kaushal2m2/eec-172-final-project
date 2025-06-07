// Monochrome bitmap animation data for cursor.gif
// Size: 20x30 pixels, 3 frames
#include "simplelink.h"
#include <string.h>

#define OPTIONBACKGROUND_WIDTH 128
#define OPTIONBACKGROUND_HEIGHT 128
#define OPTIONBACKGROUND_FRAME_COUNT 8
#define OPTIONBACKGROUND_FRAME_SIZE 2048  // Size of each frame in bytes

// Function to get a pointer to a specific frame
const uint8_t* get_optionBackground_frame(uint16_t frame_index) {
    static uint8_t frameBuffer[OPTIONBACKGROUND_FRAME_SIZE];

    // Ensure valid frame index
    if (frame_index >= OPTIONBACKGROUND_FRAME_COUNT) {
        frame_index = 0;
    }

    // Format filename
    char filename[64];
    sprintf(filename, "/optionBackgroundFrames_%d.bin", frame_index);

    // Try opening the file - cast to unsigned char*
    long fileHandle;
    int status = sl_FsOpen((unsigned char*)filename, FS_MODE_OPEN_READ, NULL, &fileHandle);

    if (status < 0) {
        // Default pattern
        memset(frameBuffer, 0, OPTIONBACKGROUND_FRAME_SIZE);
        frameBuffer[3] = 0x08;  // Default dot
        return frameBuffer;
    }

    // Read data
    long bytesRead = sl_FsRead(fileHandle, 0, frameBuffer, OPTIONBACKGROUND_FRAME_SIZE);
    sl_FsClose(fileHandle, 0, 0, 0);

    return frameBuffer;
}
