//*****************************************************************************
// Simple Video Game Demo
// Circle with physics and jumping controlled by analog stick
//*****************************************************************************

#include <shared_defs.h>

#include "pin.h"
#include "adc.h"
#include "hw_adc.h"
#include "prcm.h"
#include "uart_if.h"
#include "i2c_if.h"

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "video_game.h"
#include "character_run_right_bitmap.h"
#include "character_run_left_bitmap.h"
#include "character_jump_bitmap.h"
#include "character_double_jump_bitmap.h"
#include "map_bitmap.h"

// Display settings
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           128
#define SCREEN_CENTER_X         (SCREEN_WIDTH / 2)
#define SCREEN_CENTER_Y         (SCREEN_HEIGHT / 2)

// Circle parameters
#define CIRCLE_RADIUS           8
#define PLAYER_GROUND_COLOR            GREEN
#define PLAYER_COLOR            CYAN



// Physics parameters
#define GRAVITY                 -0.5f   // Gravity strength
#define JUMP_VELOCITY          6.0f  // Initial jump velocity
#define HORIZONTAL_ACCEL        1.0f   // Horizontal acceleration from stick
#define MAX_HORIZONTAL_SPEED    8.0f   // Maximum horizontal speed
#define HORIZONTAL_DAMPING      0.92f  // Horizontal velocity damping factor
#define GROUND_LEVEL            (CIRCLE_RADIUS + 1) // Ground level

// Button 2 pin for exit detection
#define BUTTON2_PIN             0x20     // PIN_21
#define BUTTON2_PORT            GPIOA1_BASE

// Button 1 pin for jumping
#define BUTTON1_PIN             0x40    // PIN_15
#define BUTTON1_PORT            GPIOA2_BASE

// Global variables
static float g_playerX = SCREEN_CENTER_X;
static float g_playerY = SCREEN_CENTER_Y;
static float g_playerVX = 0.0f;        // Horizontal velocity
static float g_playerVY = 0.0f;        // Vertical velocity
static int g_prevPlayerX = -1;         // Used to erase previous frame
static int g_prevPlayerY = -1;         // Used to erase previous frame
static bool g_firstFrame = true;
static bool g_isOnGround = false;      // Is player touching the ground?
static bool g_wasButton1Pressed = false; // For button state tracking
static unsigned long g_lastJumpTime = 0; // Time tracking for jump cooldown
static float character_Frame = 0;
static bool playing_jump_animation = false;
static bool double_jump_available = true;
static bool playing_double_jump_animation = false;
static int g_currentMapFrame = 0;



//*****************************************************************************
// Read ADC Channel and calculate voltage
//*****************************************************************************
static void ReadADCChannel(unsigned int uiChannel, float *voltageResult)
{
    unsigned int uiIndex = 0;
    unsigned long ulSample;
    float avgVoltage = 0.0;

    // Enable ADC channel
    MAP_ADCChannelEnable(ADC_BASE, uiChannel);

    // Sample 10 times and average
    uiIndex = 0;
    while(uiIndex < 10)
    {
        if(MAP_ADCFIFOLvlGet(ADC_BASE, uiChannel))
        {
            ulSample = MAP_ADCFIFORead(ADC_BASE, uiChannel);
            // Calculate voltage: (ADC value * 1.4V reference) / 4096 (12-bit resolution)
            avgVoltage += (((float)((ulSample >> 2) & 0x0FFF)) * 1.4) / 4096;
            uiIndex++;
        }
    }

    // Disable ADC channel
    MAP_ADCChannelDisable(ADC_BASE, uiChannel);

    // Calculate average voltage
    *voltageResult = avgVoltage / 10.0;
}

//*****************************************************************************
// Check if button 2 is pressed to exit the application
//*****************************************************************************
static bool ShouldExit(void)
{
    // Read the current button state (active low)
    return !(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0);
}

//*****************************************************************************
// Check if button 1 is pressed for jump
//*****************************************************************************
static bool IsJumpButtonPressed(void)
{
    // Read the current button state (active low)
    return !(GPIOPinRead(BUTTON1_PORT, BUTTON1_PIN) == 0);
}

//*****************************************************************************
// Get current system time in milliseconds
//*****************************************************************************
static unsigned long GetCurrentTimeMs(void)
{
    // Simplified - in a real application, you'd use a proper time source
    static unsigned long counter = 0;
    counter += 16; // Approximate milliseconds per frame at 60fps
    return counter;
}

//*****************************************************************************
// Bitmap Collision Detection
//*****************************************************************************

// Check if the player collides with any pixels in the bitmap
static void CheckBitmapCollision(int bitmapX, int bitmapY, const uint8_t *bitmap,
                                int width, int height, int pixelSize)
{
    // Convert floating point player position to integers for collision checking
    int playerX = (int)g_playerX;
    int playerY = (int)g_playerY;
    int playerWidth = CHARACTER_RUN_LEFT_WIDTH;
    int playerHeight = CHARACTER_RUN_LEFT_HEIGHT;

    // Account for character model being positioned above the collision box
    int collisionBoxOffset = playerHeight;
    int collisionY = playerY - collisionBoxOffset;

    // Early exit if no bounding box intersection is possible
    if (playerX + playerWidth < bitmapX || playerX > bitmapX + width * pixelSize ||
        collisionY > bitmapY + height * pixelSize || collisionY + playerHeight < bitmapY) {
        return;
    }

    // Calculate collision flags and positions
    bool collideTop = false;
    bool collideBottom = false;
    bool collideLeft = false;
    bool collideRight = false;
    int topCollisionY = 0;
    int bottomCollisionY = 0;
    int leftCollisionX = 0;
    int rightCollisionX = 0;

    // Get player's edges in screen coordinates with the offset applied
    int playerBottom = collisionY;
    int playerTop = collisionY + playerHeight;
    int playerLeft = playerX;
    int playerRight = playerX + playerWidth;

    int byteWidth = (width + 7) / 8; // Bytes per row in bitmap

    // Height threshold for horizontal collisions - pixels must extend this far above player's feet
    int horizontalCollisionHeightThreshold = playerHeight / 3;

    // Variable declarations outside of for loops
    int j;
    int i;
    int byteIndex;
    uint8_t bitMask;
    int pixelScreenX;
    int pixelScreenY;
    int pixelTop;
    int pixelBottom;

    // Check bitmap pixels that might intersect with player
    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            // Calculate byte index and bit mask using same logic as drawBitmap
            byteIndex = j * byteWidth + i / 8;
            bitMask = 0x80 >> (i & 7); // Start with leftmost bit

            // If this bit is set (solid pixel)
            if (bitmap[byteIndex] & bitMask) {
                // Convert bitmap pixel to screen coordinates
                pixelScreenX = bitmapX + i * pixelSize;

                // Invert Y coordinate to match screen coordinates
                pixelScreenY = bitmapY + (height - 1 - j) * pixelSize;

                // In our coordinate system, top is higher Y, bottom is lower Y
                pixelBottom = pixelScreenY;
                pixelTop = pixelScreenY + pixelSize;

                // IMPROVED: Bottom collision detection with greater vertical tolerance
                // Player landing on pixel or slightly above it (within 8 pixels)
                if (g_playerVY <= 0 && // Player falling downward or stationary
                    playerBottom >= pixelTop - 8 && // Increased tolerance
                    playerBottom <= pixelTop + 5 && // Allow small overlap
                    playerRight > pixelScreenX &&
                    playerLeft < pixelScreenX + pixelSize) {

                    collideBottom = true;
                    if (pixelTop > bottomCollisionY) {
                        bottomCollisionY = pixelTop;
                    }
                }

                // Top collision (player hitting head on pixel)
                else if (g_playerVY > 0 && // Moving upward
                         playerTop >= pixelBottom &&
                         playerTop <= pixelBottom + 5 &&
                         playerRight > pixelScreenX &&
                         playerLeft < pixelScreenX + pixelSize) {

                    collideTop = true;
                    if (topCollisionY == 0 || pixelBottom < topCollisionY) {
                        topCollisionY = pixelBottom;
                    }
                }

                // Calculate pixel height relative to player's bottom
                int pixelHeightAbovePlayerFeet = pixelTop - playerBottom;

                // Right collision (player moving right into pixel)
                if (g_playerVX > 0 &&
                    playerRight >= pixelScreenX &&
                    playerRight <= pixelScreenX + 5 &&
                    playerBottom < pixelTop &&
                    playerTop > pixelBottom &&
                    pixelHeightAbovePlayerFeet >= horizontalCollisionHeightThreshold) {

                    collideRight = true;
                    if (rightCollisionX == 0 || pixelScreenX < rightCollisionX) {
                        rightCollisionX = pixelScreenX;
                    }
                }

                // Left collision (player moving left into pixel)
                else if (g_playerVX < 0 &&
                         playerLeft <= pixelScreenX + pixelSize &&
                         playerLeft >= pixelScreenX + pixelSize - 5 &&
                         playerBottom < pixelTop &&
                         playerTop > pixelBottom &&
                         pixelHeightAbovePlayerFeet >= horizontalCollisionHeightThreshold) {

                    collideLeft = true;
                    if (leftCollisionX == 0 || pixelScreenX + pixelSize > leftCollisionX) {
                        leftCollisionX = pixelScreenX+2;
                    }
                }
            }
        }
    }

    // FIXED: Resolve collisions with improved ground state management

    // Bottom collision - player is on top of a platform
    if (collideBottom) {
        // Apply the offset when setting the player's position
        g_playerY = bottomCollisionY + collisionBoxOffset;
        g_playerVY = 0;
        g_isOnGround = true;

        // NEW: Add a small downward velocity to keep the player grounded
        // This helps prevent oscillation
        g_playerVY = -0.01f;
    }
    else{
        g_isOnGround = false;
    }
    // NOTE: Removed the else clause that was immediately setting g_isOnGround to false
    // This allows the ground state to persist between frames

    // Top collision - player hits head
    if (collideTop) {
        // Apply the offset when setting the player's position
        g_playerY = topCollisionY - playerHeight + collisionBoxOffset;
        g_playerVY = 0;
    }

    // Right collision - player hits wall on right
    if (collideRight) {
        g_playerX = rightCollisionX - playerWidth;
        g_playerVX = 0;
    }

    // Left collision - player hits wall on left
    else if (collideLeft) {
        g_playerX = leftCollisionX;
        g_playerVX = 0;
    }
}

//*****************************************************************************
// Killbox definitions
// ****************************************************************************
#define MAX_KILLBOXES 10  // Maximum number of killboxes per level

// Structure to represent a killbox
typedef struct {
    int x;          // X position of killbox
    int y;          // Y position of killbox
    int width;      // Width of killbox
    int height;     // Height of killbox
} Killbox;

// Array of killboxes for the current map
static Killbox g_killboxes[MAX_KILLBOXES];
static int g_killboxCount = 0;

// Clear all killboxes
static void ClearKillboxes(void) {
    g_killboxCount = 0;
}

// Add a killbox to the current map
static void AddKillbox(int x, int y, int width, int height) {
    if (g_killboxCount < MAX_KILLBOXES) {
        g_killboxes[g_killboxCount].x = x;
        g_killboxes[g_killboxCount].y = y;
        g_killboxes[g_killboxCount].width = width;
        g_killboxes[g_killboxCount].height = height;
        g_killboxCount++;
    }
}

//*****************************************************************************
// Check if player is in a killbox hitbox
//*****************************************************************************
static bool CheckKillboxEntry(void) {
    // Convert player position to collision box
    int playerX = (int)g_playerX;
    int playerY = (int)g_playerY;
    int playerWidth = CHARACTER_RUN_LEFT_WIDTH;
    int playerHeight = CHARACTER_RUN_LEFT_HEIGHT;

    // Account for character model being positioned
    int collisionBoxOffset = playerHeight;
    int collisionY = playerY - collisionBoxOffset;

    // Check each killbox
    int i = 0;
    for (i = 0; i < g_killboxCount; i++) {
        Killbox* killbox = &g_killboxes[i];

        // Check if player overlaps with killbox
        if (playerX < killbox->x + killbox->width &&
            playerX + playerWidth > killbox->x &&
            collisionY < killbox->y + killbox->height &&
            collisionY + playerHeight > killbox->y) {

            // Player is in killbox - reset to starting position
            return true;  // Killbox hit
        }
    }

    return false;  // No killbox hit
}

//*****************************************************************************
// Door definitions
//*****************************************************************************
#define MAX_DOORS 5  // Maximum number of doors per level

// Structure to represent a door
typedef struct {
    int x;          // X position of door
    int y;          // Y position of door
    int width;      // Width of door
    int height;     // Height of door
    int targetMap;  // Map frame to load when door is entered
} Door;

// Array of doors for the current map
static Door g_doors[MAX_DOORS];
static int g_doorCount = 0;

// Clear all doors
static void ClearDoors(void) {
    g_doorCount = 0;
}

// Add a door to the current map
static void AddDoor(int x, int y, int width, int height, int targetMap) {
    if (g_doorCount < MAX_DOORS) {
        g_doors[g_doorCount].x = x;
        g_doors[g_doorCount].y = y;
        g_doors[g_doorCount].width = width;
        g_doors[g_doorCount].height = height;
        g_doors[g_doorCount].targetMap = targetMap;
        g_doorCount++;
    }
}

//*****************************************************************************
// Check if player is in a door hitbox
//*****************************************************************************
static bool CheckDoorEntry(void) {
    // Convert player position to collision box
    int playerX = (int)g_playerX;
    int playerY = (int)g_playerY;
    int playerWidth = CHARACTER_RUN_LEFT_WIDTH;
    int playerHeight = CHARACTER_RUN_LEFT_HEIGHT;

    // Account for character model being positioned
    int collisionBoxOffset = playerHeight;
    int collisionY = playerY - collisionBoxOffset;

    // Check each door
    int i = 0;
    for (i = 0; i < g_doorCount; i++) {
        Door* door = &g_doors[i];

        // Check if player overlaps with door
        if (playerX < door->x + door->width &&
            playerX + playerWidth > door->x &&
            collisionY < door->y + door->height &&
            collisionY + playerHeight > door->y) {

            // Player is in door - switch to target map frame
            if (g_currentMapFrame != door->targetMap) {
                g_currentMapFrame = door->targetMap;
                return true;  // Map changed
            }
        }
    }

    return false;  // No door entered
}

//*****************************************************************************
// Enemy definitions
//*****************************************************************************
#define MAX_ENEMIES 8  // Maximum number of enemies per level

// Structure to represent an enemy
typedef struct {
    float x;                    // Current X position
    float y;                    // Fixed Y position
    float x1;                   // Left boundary
    float x2;                   // Right boundary
    float speed;                // Movement speed
    int direction;              // Current direction: 1 = right, -1 = left
    float characterFrame;       // Animation frame for walk cycle
    int prevX;                 // Previous X for erasing
    int prevY;                 // Previous Y for erasing
    bool firstDraw;            // First draw flag
} Enemy;



// Array of enemies for the current map
static Enemy g_enemies[MAX_ENEMIES];
static int g_enemyCount = 0;

// Enemy movement parameters
#define ENEMY_SPEED 1.0f       // Constant horizontal speed

//*****************************************************************************
// Enemy management functions
//*****************************************************************************

// Clear all enemies
static void ClearEnemies(void) {
    g_enemyCount = 0;
}

// Add an enemy to the current map
static void AddEnemy(float x, float y, float x1, float x2, float speed, int initialDirection) {
    if (g_enemyCount < MAX_ENEMIES) {
        g_enemies[g_enemyCount].x = x;
        g_enemies[g_enemyCount].y = y;
        g_enemies[g_enemyCount].x1 = x1;
        g_enemies[g_enemyCount].x2 = x2;
        g_enemies[g_enemyCount].speed = speed;
        g_enemies[g_enemyCount].direction = initialDirection; // 1 for right, -1 for left
        g_enemies[g_enemyCount].characterFrame = 0.0f;
        g_enemies[g_enemyCount].prevX = -1;
        g_enemies[g_enemyCount].prevY = -1;
        g_enemies[g_enemyCount].firstDraw = true;
        g_enemyCount++;
    }
}

//*****************************************************************************
// Check if player collides with any enemy
//*****************************************************************************
static bool CheckPlayerEnemyCollision(void) {
    // Convert player position to collision box
    int playerX = (int)g_playerX;
    int playerY = (int)g_playerY;
    int playerWidth = CHARACTER_RUN_LEFT_WIDTH;
    int playerHeight = CHARACTER_RUN_LEFT_HEIGHT;

    // Account for character model being positioned
    int collisionBoxOffset = playerHeight;
    int collisionY = playerY - collisionBoxOffset;

    // Check each enemy
    int i = 0;
    for (i = 0; i < g_enemyCount; i++) {
        Enemy* enemy = &g_enemies[i];

        // Convert enemy position to collision box
        int enemyX = (int)enemy->x;
        int enemyY = (int)enemy->y;
        int enemyCollisionY = enemyY - collisionBoxOffset;

        // Check if player overlaps with enemy
        if (playerX < enemyX + playerWidth &&
            playerX + playerWidth > enemyX &&
            collisionY < enemyCollisionY + playerHeight &&
            collisionY + playerHeight > enemyCollisionY) {

            // Player hit enemy - reset like killbox
            return true;
        }
    }

    return false;  // No enemy hit
}


//*****************************************************************************
// Update enemy physics and AI
//*****************************************************************************
static void UpdateEnemyPhysics(void) {
    int i = 0;
    for (i = 0; i < g_enemyCount; i++) {
        Enemy* enemy = &g_enemies[i];

        // Move enemy based on current direction and speed
        enemy->x += enemy->direction * enemy->speed;

        // Check boundaries and reverse direction if needed
        if (enemy->x <= enemy->x1) {
            enemy->x = enemy->x1;
            enemy->direction = 1; // Move right
        }
        else if (enemy->x >= enemy->x2) {
            enemy->x = enemy->x2;
            enemy->direction = -1; // Move left
        }
    }
}

//*****************************************************************************
// Update enemy animations based on movement
//*****************************************************************************
static void UpdateEnemyAnimations(void) {
    int i = 0;
    for (i = 0; i < g_enemyCount; i++) {
        Enemy* enemy = &g_enemies[i];

        // Update animation frame based on movement speed and direction
        // Animation speed scales with movement speed for more realistic motion
        float animationSpeed = (enemy->speed / MAX_HORIZONTAL_SPEED) * 2.0f;

        if (enemy->direction > 0) {
            // Moving right - advance right animation
            enemy->characterFrame += animationSpeed;
            if (enemy->characterFrame >= CHARACTER_RUN_RIGHT_FRAME_COUNT) {
                enemy->characterFrame = 0; // Loop back to start
            }
        } else {
            // Moving left - advance left animation
            enemy->characterFrame += animationSpeed;
            if (enemy->characterFrame >= CHARACTER_RUN_LEFT_FRAME_COUNT) {
                enemy->characterFrame = 0; // Loop back to start
            }
        }
    }
}


//*****************************************************************************
// Draw all enemies
//*****************************************************************************
static void DrawEnemies(void) {
    int i = 0;
    for (i = 0; i < g_enemyCount; i++) {
        Enemy* enemy = &g_enemies[i];

        // Select bitmap based on movement direction and current animation frame
        const uint8_t* enemy_bitmap;
        if (enemy->direction > 0) {
            // Moving right - use current frame of running right animation
            enemy_bitmap = get_character_run_right_frame((int)enemy->characterFrame);
        } else {
            // Moving left - use current frame of running left animation
            enemy_bitmap = get_character_run_left_frame((int)enemy->characterFrame);
        }

        // Erase previous enemy position if not first frame
        if (!enemy->firstDraw) {
            DrawCharacter(enemy->prevX, enemy->prevY, enemy_bitmap, BLACK, true, BLACK);
        } else {
            enemy->firstDraw = false;
        }

        // Draw enemy at new position using RED color to distinguish from player
        DrawCharacter((int)enemy->x, (int)enemy->y, enemy_bitmap, RED, false, BLACK);

        // Save current position for next frame
        enemy->prevX = (int)enemy->x;
        enemy->prevY = (int)enemy->y;
    }
}

//*****************************************************************************
// Update player physics based on input and environment
//*****************************************************************************
static void UpdatePlayerPhysics(void)
{
    float voltage_59, voltage_60;
    bool jumpButtonPressed = IsJumpButtonPressed();
    unsigned long currentTime = GetCurrentTimeMs();

    // Read X-axis from ADC for horizontal control
    ReadADCChannel(ADC_CH_2, &voltage_59);

    // Apply horizontal acceleration based on analog stick
    if(fabs(((voltage_59)/1.4)-.5) >= .1){
        g_playerVX -= (((voltage_59)/1.4)-.5)*(HORIZONTAL_ACCEL);
    }

    // Apply horizontal speed limit
    if (g_playerVX > MAX_HORIZONTAL_SPEED) {
        g_playerVX = MAX_HORIZONTAL_SPEED;
    } else if (g_playerVX < -MAX_HORIZONTAL_SPEED) {
        g_playerVX = -MAX_HORIZONTAL_SPEED;
    }

    // Handle jumping with Button 1
    if (jumpButtonPressed && !g_wasButton1Pressed && (currentTime - g_lastJumpTime > 50) && (g_isOnGround || double_jump_available)) {
        if(!g_isOnGround){
           double_jump_available = false;
           playing_double_jump_animation = true;
        }
        else{
            playing_jump_animation = true;
        }

        g_isOnGround = false;
        g_lastJumpTime = currentTime;
        character_Frame = 0;
        g_playerVY = JUMP_VELOCITY;
    }

    if(g_isOnGround){
        double_jump_available = true;
    }

    // Update button state
    g_wasButton1Pressed = jumpButtonPressed;

    // Apply gravity
    if (!g_isOnGround) {
        g_playerVY += GRAVITY;
    }

    // Apply horizontal damping
    g_playerVX *= HORIZONTAL_DAMPING;

    // Update position based on velocity
    g_playerX += g_playerVX;
    g_playerY += g_playerVY;

    // Check enemy collision - if hit, reset the player
    if (CheckPlayerEnemyCollision()) {
        // Player hit an enemy - reset like killbox/falling off map
        g_firstFrame = true;
        VideoGame_Initialize();
        return;
    }

    // Check killbox entry - if hit, reset the player
    if (CheckKillboxEntry()) {
        // Player hit a killbox - reset like falling off map
        g_firstFrame = true;
        VideoGame_Initialize();
        return;
    }

    // Check door entry - if entered a door, return early to let map change take effect
    if (CheckDoorEntry()) {
        // Door entered, map changed - reinitialize the game with the new map
        VideoGame_Initialize();
        return;
     }

    // Check collision with the level bitmap
    const uint8_t* levelBitmap = get_map_frame(g_currentMapFrame);
    static int levelWidth = 128;
    static int levelHeight = 128;
    static int levelX = 0;
    static int levelY = 0;

    // If levelBitmap is set, check collisions with it
    if (levelBitmap != NULL) {
        CheckBitmapCollision(levelX, levelY, levelBitmap, levelWidth, levelHeight, 1);
    }

    // Boundary detection for left/right (fallback collision)
    if (g_playerX < 0) {
        g_playerX = 0;
        g_playerVX = 0;
    } else if (g_playerX > SCREEN_WIDTH - CHARACTER_RUN_LEFT_WIDTH) {
        g_playerX = SCREEN_WIDTH - CHARACTER_RUN_LEFT_WIDTH;
        g_playerVX = 0;
    }

    // Check if player has fallen off the bottom of the screen
    if (g_playerY < 0) {
        g_firstFrame = true;
        VideoGame_Initialize();
        return;
    }

    // Ceiling collision
    if (g_playerY > SCREEN_HEIGHT) {
        g_playerY = SCREEN_HEIGHT;
        g_playerVY = 0;
    }
}


//*****************************************************************************
// Initialize the video game and set up the level
//*****************************************************************************
void VideoGame_Initialize(void)
{
    // Clear the screen
    fillScreen(BLACK);

    // Set up the current map frame
    const uint8_t* levelBitmap = get_map_frame(g_currentMapFrame);
    drawBitmap(0, 0, levelBitmap, 128, 128, WHITE, 1, false, BLACK);

    // Clear and set up doors for the current map
    ClearDoors();

    // Clear and set up killboxes for the current map
    ClearKillboxes();

    // Clear and set up enemies for the current map
    ClearEnemies();

    // Configure doors, killboxes, and enemies based on the current map frame
    if (g_currentMapFrame == 0) {
        // Add doors for map 1
        AddDoor(100, 80, 20, 30, 1);  // Door to map 2 at position x=100, y=80


        // Add enemies for map 1
    }
    else if (g_currentMapFrame == 1) {
        // Add doors for map 2
        AddDoor(10, 86, 10, 24, 0);   // Door back to map 1
        AddDoor(118, 11, 10, 24, 2);  // Door to map 3
        AddKillbox(64, 64, 10, 55);   // Spike hazard

    }
    else if (g_currentMapFrame == 2) {
        // Add doors for map 3
        AddDoor(0, 8, 10, 24, 1);   // Door back to map 1
        AddDoor(101, 94, 10, 24, 3);  // Door to map 3
        AddKillbox(0, 70, 8, 54);    // Left spike hazard
        AddKillbox(93, 60, 33, 8);    // Right spike hazard
        AddEnemy(63, 55, 63, 79, 1.0f, 1);     // Enemy patrolling middle area

    }
    else if (g_currentMapFrame == 3) {
            // Add doors for map 3
         AddDoor(10, 93, 10, 24, 2);   // Door back to map 4
         AddDoor(118, 11, 10, 90, 4);   // Door to map 5
         AddEnemy(0, 33, 0, 54, 1.0f, 1); // Left enemy
         AddEnemy(86, 33, 86, 118, 1.0f, 1); // Right enemy
    }
    else if (g_currentMapFrame == 4) {
            // Add doors for map 3
         AddDoor(0, 20, 10, 24, 3);   // Door back to map 4
         AddDoor(118, 40, 10, 50, 5);   // Door to map 6
         AddEnemy(62, 55, 62, 85, 1.0f, 1); // stair enemy
    }
    else if (g_currentMapFrame == 5) {
                // Add doors for map 3
         AddDoor(0, 48, 3, 50, 4);   // Door back to map 5
         AddKillbox(0, 95, 32, 9);    // Upper hazard Right
         AddKillbox(95, 95, 32, 9);    // Upper hazard Left
         AddKillbox(0, 0, 127, 30);    // Upper hazard Left
         AddDoor(123, 48, 4, 50, 0);   // Door back to map 5
         AddEnemy(46, 95, 46, 71, 1.0f, 1); // Left enemy
         AddEnemy(94, 67, 94, 118, 1.0f, 1); // Right enemy
    }

    // Initialize player position - can be different for each map
    if (g_currentMapFrame == 0) {
            g_playerX = 60;
            g_playerY = 80;
        }
    if (g_currentMapFrame == 1) {
        g_playerX = 20;
        g_playerY = 107;
    }
    else if (g_currentMapFrame == 2) {
        g_playerX = 20;
        g_playerY = 35;
    }
    if (g_currentMapFrame == 3) {
        g_playerX = 26;
        g_playerY = 112;
    }
    if (g_currentMapFrame == 4) {
        g_playerX = 12;
        g_playerY = 41;
    }
    else if (g_currentMapFrame == 5) {
        // Position player near the entry door
        g_playerX = 8;
        g_playerY = 73;
   }

    // Initialize other player variables
    g_playerVX = 0.0f;
    g_playerVY = 0.0f;
    g_prevPlayerX = -1;  // Invalid position to force first draw
    g_prevPlayerY = -1;
    g_isOnGround = false;
    g_wasButton1Pressed = false;
    g_lastJumpTime = 0;
    character_Frame = 1;
    playing_jump_animation = false;
    playing_double_jump_animation = false;
    double_jump_available = true;
}

//*****************************************************************************
// Select the appropriate bitmap based on character state and velocity
//*****************************************************************************
static const uint8_t* SelectCharacterBitmap(float vx, bool playingJumpAnimation, bool playingDoubleJumpAnimation, float characterFrame)
{
    const uint8_t* bitmap;

    // Check double jump first (it should take priority)
    if (playingDoubleJumpAnimation) {
        bitmap = get_character_double_jump_frame((int)characterFrame);
    }
    else if (playingJumpAnimation) {
        bitmap = get_character_jump_frame((int)characterFrame);
    }
    else if (vx < -1) {
        bitmap = get_character_run_left_frame((int)characterFrame);
    }
    else if (vx > 1) {
        bitmap = get_character_run_right_frame((int)characterFrame);
    }
    else {
        // Idle - use appropriate frame
        bitmap = (vx >= 0) ? get_character_run_right_frame(3) : get_character_run_left_frame(3);
    }

    return bitmap;
}

// Update animation state and frame count
static void UpdateCharacterAnimation(float vx, bool isOnGround, bool* playingJumpAnimationPtr,
                                    bool* playingDoubleJumpAnimationPtr, float* characterFramePtr)
{
    // Handle double jump animation (check first for priority)
    if (*playingDoubleJumpAnimationPtr) {
        *characterFramePtr += 0.35;
        if(*characterFramePtr >= CHARACTER_DOUBLE_JUMP_FRAME_COUNT - 1) {
            // Hold on last frame until landing
            *characterFramePtr = CHARACTER_DOUBLE_JUMP_FRAME_COUNT - 1;

            if (isOnGround) {
                *playingDoubleJumpAnimationPtr = false;
                *characterFramePtr = 0;
            }
        }
    }
    // Handle regular jump animation
    else if (*playingJumpAnimationPtr) {
        if (*characterFramePtr < CHARACTER_JUMP_FRAME_COUNT - 1) {
            *characterFramePtr += 0.35;
        }

        if (isOnGround) {
            *playingJumpAnimationPtr = false;
            *characterFramePtr = 0;
        }
    }

    // Handle running animations when on ground and moving
    else if (isOnGround) {
        if (vx < -0.5) {
            // Running left animation
            *characterFramePtr -= (2*vx)/MAX_HORIZONTAL_SPEED;
            if (*characterFramePtr > CHARACTER_RUN_LEFT_FRAME_COUNT) {
                *characterFramePtr = 0;
            }
        }
        else if (vx > 0.5) {
            // Running right animation
            *characterFramePtr += (2*vx)/MAX_HORIZONTAL_SPEED;
            if (*characterFramePtr > CHARACTER_RUN_RIGHT_FRAME_COUNT) {
                *characterFramePtr = 0;
            }
        }
        else {
            // Reset animation when not moving
            *characterFramePtr = 0;
        }
    }
}

// Draw character with the correct Y-coordinate transformation
static void DrawCharacter(int x, int y, const uint8_t* bitmap, uint16_t color, bool eraseMode, uint16_t bgColor)
{
    // Apply transformation for Y coordinate
    int screenY = SCREEN_HEIGHT - y;

    // Draw bitmap with transformation
    drawBitmap(x, screenY, bitmap, CHARACTER_RUN_LEFT_WIDTH, CHARACTER_RUN_LEFT_HEIGHT,
               color, 1, eraseMode, bgColor);
}

//*****************************************************************************
// Run one frame of the video game
//*****************************************************************************
bool VideoGame_RunFrame(void)
{
    bool debugview = false;
    // Check if button 2 is pressed to exit
    if(ShouldExit()) {
        g_firstFrame = true;
        return false;
    }

    if(g_firstFrame){
        VideoGame_Initialize();
    }

    // Update player physics
    UpdatePlayerPhysics();



    // Update enemy physics and AI
    UpdateEnemyPhysics();

    // Check if player has fallen off screen
    if (g_firstFrame && g_playerY < 0) {
        return false;
    }

    if(debugview){
    // Drawing doors
    int i = 0;
    for (i = 0; i < g_doorCount; i++) {
        Door* door = &g_doors[i];
        drawRect(door->x, SCREEN_HEIGHT - door->y - door->height,
                door->width, door->height, MAGENTA);
    }

    // Drawing killboxes
    for (i = 0; i < g_killboxCount; i++) {
        Killbox* killbox = &g_killboxes[i];
        drawRect(killbox->x, SCREEN_HEIGHT - killbox->y - killbox->height,
                killbox->width, killbox->height, RED);
    }
    }


    // Update enemy animations
    UpdateEnemyAnimations();
    // Draw enemies
     DrawEnemies();

    // Get the appropriate character bitmap based on state
    const uint8_t* character_bitmap = SelectCharacterBitmap(g_playerVX, playing_jump_animation,
                                                               playing_double_jump_animation, character_Frame);

    // Erase previous player position if not first frame
    if (!g_firstFrame) {
        DrawCharacter((int)g_prevPlayerX, (int)g_prevPlayerY, character_bitmap, BLACK, true, BLACK);
    } else {
        g_firstFrame = false;
    }

    // Update character animation state
    UpdateCharacterAnimation(g_playerVX, g_isOnGround, &playing_jump_animation,
                                &playing_double_jump_animation, &character_Frame);

    // Get the updated character bitmap after animation update
    character_bitmap = SelectCharacterBitmap(g_playerVX, playing_jump_animation,
                                                playing_double_jump_animation, character_Frame);

    // Draw player
    if(g_isOnGround){
        DrawCharacter((int)g_playerX, (int)g_playerY, character_bitmap, PLAYER_GROUND_COLOR, false, BLACK);
    }
    else{
        DrawCharacter((int)g_playerX, (int)g_playerY, character_bitmap, PLAYER_COLOR, false, BLACK);
    }

    // Save current position for next frame
    g_prevPlayerX = (int)g_playerX;
    g_prevPlayerY = (int)g_playerY;


    return true;
}
