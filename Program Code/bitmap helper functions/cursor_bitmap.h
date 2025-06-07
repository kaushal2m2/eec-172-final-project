#include "simplelink.h"
#include <string.h>

#define CURSOR_WIDTH 20
#define CURSOR_HEIGHT 30
#define CURSOR_FRAME_COUNT 3
#define CURSOR_FRAME_SIZE 90  // Size of each frame in bytes


// Function to get a pointer to a specific frame
const uint8_t* get_cursor_frame(uint16_t frame_index) {
    static uint8_t frameBuffer[CURSOR_FRAME_SIZE];

    // Ensure valid frame index
    if (frame_index >= CURSOR_FRAME_COUNT) {
        frame_index = 0;
    }

    // Format filename
    char filename[64];
    sprintf(filename, "/cursorFrames_%d.bin", frame_index);

    // Try opening the file - cast to unsigned char*
    long fileHandle;
    int status = sl_FsOpen((unsigned char*)filename, FS_MODE_OPEN_READ, NULL, &fileHandle);

    if (status < 0) {
        // Default pattern
        memset(frameBuffer, 0, CURSOR_FRAME_SIZE);
        frameBuffer[3] = 0x08;  // Default dot
        return frameBuffer;
    }

    // Read data
    long bytesRead = sl_FsRead(fileHandle, 0, frameBuffer, CURSOR_FRAME_SIZE);
    sl_FsClose(fileHandle, 0, 0, 0);

    return frameBuffer;
}
