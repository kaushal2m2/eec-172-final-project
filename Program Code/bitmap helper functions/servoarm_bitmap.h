
#include "simplelink.h"
#include <string.h>

#define SERVOARM_WIDTH 128
#define SERVOARM_HEIGHT 128
#define SERVOARM_FRAME_COUNT 2
#define SERVOARM_FRAME_SIZE 2048  // Size of each frame in bytes


// Function to get a pointer to a specific frame
const uint8_t* get_servoarm_frame(uint16_t frame_index) {
    static uint8_t frameBuffer[SERVOARM_FRAME_SIZE];

    // Ensure valid frame index
    if (frame_index >= SERVOARM_FRAME_COUNT) {
        frame_index = 0;
    }

    // Format filename
    char filename[64];
    sprintf(filename, "/servoarmFrames_0.bin", frame_index);

    // Try opening the file - cast to unsigned char*
    long fileHandle;
    int status = sl_FsOpen((unsigned char*)filename, FS_MODE_OPEN_READ, NULL, &fileHandle);

    if (status < 0) {
        // Default pattern
        memset(frameBuffer, 0, SERVOARM_FRAME_SIZE);
        frameBuffer[3] = 0x08;  // Default dot
        return frameBuffer;
    }

    // Read data
    long bytesRead = sl_FsRead(fileHandle, 0, frameBuffer, SERVOARM_FRAME_SIZE);
    sl_FsClose(fileHandle, 0, 0, 0);

    return frameBuffer;
}
