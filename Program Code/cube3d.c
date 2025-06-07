//*****************************************************************************
// 3D Cube implementation using accelerometer data for orientation control
// Based on BMA222 accelerometer and SSD1351 OLED for TI CC3200
// Added wireframe environment walls visualization
//*****************************************************************************

// Standard includes

#include <shared_defs.h>
#include "pin.h"


#define SPI_IF_BIT_RATE  20000000

//*****************************************************************************
//                      MACRO DEFINITIONS
//*****************************************************************************
#define APPLICATION_VERSION     "1.4.0"
#define APP_NAME                "3D Cube with Accelerometer"
#define UART_PRINT              Report
#define FOREVER                 1
#define CONSOLE                 UARTA0_BASE
#define FAILURE                 -1
#define SUCCESS                 0
#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}

#define BUTTON2_PIN     0x20     // PIN_21
#define BUTTON2_PORT    GPIOA1_BASE

// Check if button 2 is pressed to exit the application
bool Cube3D_ShouldExit(void)
{
    // Read the current button state (active low)
    return !(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0);
}

// Display settings
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           128
#define SCREEN_CENTER_X         (SCREEN_WIDTH / 2)
#define SCREEN_CENTER_Y         (SCREEN_HEIGHT / 2)

// 3D Cube parameters
#define CUBE_SIZE               15
#define NUM_VERTICES            8
#define NUM_EDGES               12

// Physics environment parameters - updated to match visual environment
#define ENVIRONMENT_SIZE      128.0f
#define ENVIRONMENT_MIN       80.0f
#define ENVIRONMENT_MAX       128.0f
#define GRAVITY_STRENGTH      1.1f    // Strength of gravity
#define DAMPING               0.98f   // Velocity damping (energy loss)
#define ANGULAR_DAMPING       0.25f   // Angular velocity damping
#define RESTITUTION           0.5f    // Bounciness (0 = no bounce, 1 = perfect bounce)
#define TIME_STEP             0.9f    // Physics time step
#define STABILIZATION_STRENGTH 0.02f  // Strength of the stabilizing torque

// Physics boundaries matching the visual environment walls
#define PHYSICS_MIN_X         (-ENV_VISUAL_SIZE)
#define PHYSICS_MAX_X         (ENV_VISUAL_SIZE)
#define PHYSICS_MIN_Y         (-ENV_VISUAL_SIZE)
#define PHYSICS_MAX_Y         (ENV_VISUAL_SIZE)
#define PHYSICS_MIN_Z         (-ENV_VISUAL_SIZE + ENV_Z_OFFSET)
#define PHYSICS_MAX_Z         (ENV_VISUAL_SIZE + ENV_Z_OFFSET)

// Environment wall colors - adjust these based on your display's color definitions
#define WALL_COLOR            0x3186  // Dark gray color (adjust as needed)

// Note: The visual environment walls now match the physics boundaries exactly

//*****************************************************************************
//                 GLOBAL VARIABLES
//*****************************************************************************
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif


// 3D Cube vertices (x, y, z)
float g_cube_vertices[NUM_VERTICES][3] = {
    {-CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE}, // 0: left-bottom-back
    { CUBE_SIZE, -CUBE_SIZE, -CUBE_SIZE}, // 1: right-bottom-back
    { CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE}, // 2: right-top-back
    {-CUBE_SIZE,  CUBE_SIZE, -CUBE_SIZE}, // 3: left-top-back
    {-CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE}, // 4: left-bottom-front
    { CUBE_SIZE, -CUBE_SIZE,  CUBE_SIZE}, // 5: right-bottom-front
    { CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE}, // 6: right-top-front
    {-CUBE_SIZE,  CUBE_SIZE,  CUBE_SIZE}  // 7: left-top-front
};

// Environment bounding box vertices (visual representation, positioned for optimal viewing)
#define ENV_VISUAL_SIZE    60.0f  // Size of the environment box
#define ENV_Z_OFFSET       60.0f  // Push environment toward back wall (higher Z = farther away)
float g_environment_vertices[NUM_VERTICES][3] = {
    {-ENV_VISUAL_SIZE, -ENV_VISUAL_SIZE, -ENV_VISUAL_SIZE + ENV_Z_OFFSET}, // 0: left-bottom-back
    { ENV_VISUAL_SIZE, -ENV_VISUAL_SIZE, -ENV_VISUAL_SIZE + ENV_Z_OFFSET}, // 1: right-bottom-back
    { ENV_VISUAL_SIZE,  ENV_VISUAL_SIZE, -ENV_VISUAL_SIZE + ENV_Z_OFFSET}, // 2: right-top-back
    {-ENV_VISUAL_SIZE,  ENV_VISUAL_SIZE, -ENV_VISUAL_SIZE + ENV_Z_OFFSET}, // 3: left-top-back
    {-ENV_VISUAL_SIZE, -ENV_VISUAL_SIZE,  ENV_VISUAL_SIZE + ENV_Z_OFFSET}, // 4: left-bottom-front
    { ENV_VISUAL_SIZE, -ENV_VISUAL_SIZE,  ENV_VISUAL_SIZE + ENV_Z_OFFSET}, // 5: right-bottom-front
    { ENV_VISUAL_SIZE,  ENV_VISUAL_SIZE,  ENV_VISUAL_SIZE + ENV_Z_OFFSET}, // 6: right-top-front
    {-ENV_VISUAL_SIZE,  ENV_VISUAL_SIZE,  ENV_VISUAL_SIZE + ENV_Z_OFFSET}  // 7: left-top-front
};

// Cube edges defined as pairs of vertex indices (same for cube and environment)
int g_cube_edges[NUM_EDGES][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, // back face
    {4, 5}, {5, 6}, {6, 7}, {7, 4}, // front face
    {0, 4}, {1, 5}, {2, 6}, {3, 7}  // connecting edges
};

// Projected 2D coordinates for cube
int g_projected_vertices[NUM_VERTICES][2];
int g_prev_projected_vertices[NUM_VERTICES][2];

// Projected 2D coordinates for environment walls
int g_projected_env_vertices[NUM_VERTICES][2];
int g_prev_projected_env_vertices[NUM_VERTICES][2];

// Added flag to track first frame
bool g_first_frame = true;

// Current rotation angles
float g_angleX = 0.0f;
float g_angleY = 0.0f;
float g_angleZ = 0.0f;

// Position of cube center (start in middle of physics environment)
float g_positionX = SCREEN_CENTER_X;
float g_positionY = SCREEN_CENTER_Y;
float g_positionZ = 40.0f;  // Start in middle of Z range (0 to +80, so middle is +40)

// Linear velocity of cube
float g_velocityX = 0.0f;
float g_velocityY = 0.0f;
float g_velocityZ = 0.0f;

// Angular velocity of cube
float g_angularVelocityX = 0.0f;
float g_angularVelocityY = 0.0f;
float g_angularVelocityZ = 0.0f;

// Frame counter
int frameCount = 0;

//*****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS
//*****************************************************************************

//*****************************************************************************
// Display banner with application information
//*****************************************************************************
static void DisplayBanner(char * AppName)
{
    Report("\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t      CC3200 %s Application       \n\r", AppName);
    Report("\t\t *************************************************\n\r");
    Report("\n\n\n\r");
}


//*****************************************************************************
// Apply rotations to a 3D point
//*****************************************************************************
void RotatePoint(float x, float y, float z, float* rx, float* ry, float* rz)
{
    // Apply X-axis rotation (pitch)
    float temp_y = y;
    float temp_z = z;
    float temp_x = x;
    y = temp_y * cosf(g_angleX) - temp_z * sinf(g_angleX);
    z = temp_y * sinf(g_angleX) + temp_z * cosf(g_angleX);

    // Apply Y-axis rotation (yaw)
    temp_x = x;
    temp_z = z;
    x = temp_x * cosf(g_angleY) + temp_z * sinf(g_angleY);
    z = -temp_x * sinf(g_angleY) + temp_z * cosf(g_angleY);

    // Apply Z-axis rotation (roll)
    temp_x = x;
    temp_y = y;
    x = temp_x * cosf(g_angleZ) - temp_y * sinf(g_angleZ);
    y = temp_x * sinf(g_angleZ) + temp_y * cosf(g_angleZ);

    *rx = x;
    *ry = y;
    *rz = z;
}

//*****************************************************************************
// Project a 3D point to 2D screen space using perspective projection
//*****************************************************************************
void ProjectPoint(float x, float y, float z, int* px, int* py)
{
    // First translate the point based on the cube's position
    x += g_positionX - SCREEN_CENTER_X;
    y += g_positionY - SCREEN_CENTER_Y;
    z += g_positionZ;

    // Perspective factor - adjust as needed
    float f = 200.0f;
    float z_offset = 100.0f;

    // Apply perspective projection only if z+z_offset is not zero or very small
    if (fabsf(z + z_offset) > 0.001f) {
        float perspective = f / (z + z_offset);
        *px = SCREEN_CENTER_X + (int)(x * perspective);
        *py = SCREEN_CENTER_Y - (int)(y * perspective);  // Y is inverted in screen space
    } else {
        *px = SCREEN_CENTER_X + (int)x;
        *py = SCREEN_CENTER_Y - (int)y;
    }
}

//*****************************************************************************
// Project environment wall point (fixed in world space)
//*****************************************************************************
void ProjectEnvironmentPoint(float x, float y, float z, int* px, int* py)
{
    // Environment walls are fixed in world space - don't translate by cube position
    // Only apply perspective projection with a fixed viewpoint

    // Perspective factor - same as cube projection
    float f = 200.0f;
    float z_offset = 100.0f;

    // Apply perspective projection
    if (fabsf(z + z_offset) > 0.001f) {
        float perspective = f / (z + z_offset);
        float projectedX = x * perspective;
        float projectedY = y * perspective;

        // Clamp to reasonable screen bounds to prevent extreme coordinates
        if (projectedX > 200.0f) projectedX = 200.0f;
        if (projectedX < -200.0f) projectedX = -200.0f;
        if (projectedY > 200.0f) projectedY = 200.0f;
        if (projectedY < -200.0f) projectedY = -200.0f;

        *px = SCREEN_CENTER_X + (int)projectedX;
        *py = SCREEN_CENTER_Y - (int)projectedY;  // Y is inverted in screen space
    } else {
        *px = SCREEN_CENTER_X + (int)x;
        *py = SCREEN_CENTER_Y - (int)y;
    }

    // Additional bounds checking for final screen coordinates
    if (*px < 0) *px = 0;
    if (*px >= SCREEN_WIDTH) *px = SCREEN_WIDTH - 1;
    if (*py < 0) *py = 0;
    if (*py >= SCREEN_HEIGHT) *py = SCREEN_HEIGHT - 1;
}

//*****************************************************************************
// Check for and resolve collisions between cube vertices and environment
//*****************************************************************************
void CheckAndResolveCollisions()
{
    int i;
    float rx, ry, rz;
    bool collisionDetected = false;
    float collisionNormalX = 0.0f, collisionNormalY = 0.0f, collisionNormalZ = 0.0f;

    // Transform and check each vertex
    for (i = 0; i < NUM_VERTICES; i++) {
        // Apply rotation
        RotatePoint(
            g_cube_vertices[i][0],
            g_cube_vertices[i][1],
            g_cube_vertices[i][2],
            &rx, &ry, &rz
        );

        // Translate to world position
        float worldX = g_positionX + rx - SCREEN_CENTER_X;  // Convert to world coordinates
        float worldY = g_positionY + ry - SCREEN_CENTER_Y;
        float worldZ = g_positionZ + rz;

        // Check collision with environment boundaries using physics bounds
        if (worldX < PHYSICS_MIN_X) {
            collisionDetected = true;
            collisionNormalX += 1.0f;  // Normal pointing right

            // Push cube back inside boundary
            float penetration = PHYSICS_MIN_X - worldX;
            g_positionX += penetration;
        }
        else if (worldX > PHYSICS_MAX_X) {
            collisionDetected = true;
            collisionNormalX += -1.0f;  // Normal pointing left

            // Push cube back inside boundary
            float penetration = worldX - PHYSICS_MAX_X;
            g_positionX -= penetration;
        }

        if (worldY < PHYSICS_MIN_Y) {
            collisionDetected = true;
            collisionNormalY += 1.0f;  // Normal pointing down

            // Push cube back inside boundary
            float penetration = PHYSICS_MIN_Y - worldY;
            g_positionY += penetration;
        }
        else if (worldY > PHYSICS_MAX_Y) {
            collisionDetected = true;
            collisionNormalY += -1.0f;  // Normal pointing up

            // Push cube back inside boundary
            float penetration = worldY - PHYSICS_MAX_Y;
            g_positionY -= penetration;
        }

        if (worldZ < PHYSICS_MIN_Z) {
            collisionDetected = true;
            collisionNormalZ += 1.0f;  // Normal pointing forward

            // Push cube back inside boundary
            float penetration = PHYSICS_MIN_Z - worldZ;
            g_positionZ += penetration;
        }
        else if (worldZ > PHYSICS_MAX_Z) {
            collisionDetected = true;
            collisionNormalZ += -1.0f;  // Normal pointing backward

            // Push cube back inside boundary
            float penetration = worldZ - PHYSICS_MAX_Z;
            g_positionZ -= penetration;
        }
    }

    // Handle collision response if any detected
    if (collisionDetected) {
        // Normalize the collision normal (could be from multiple vertices)
        float normalLength = sqrtf(collisionNormalX * collisionNormalX +
                                 collisionNormalY * collisionNormalY +
                                 collisionNormalZ * collisionNormalZ);

        if (normalLength > 0.001f) {
            collisionNormalX /= normalLength;
            collisionNormalY /= normalLength;
            collisionNormalZ /= normalLength;

            // Calculate dot product of velocity and normal
            float dotProduct = g_velocityX * collisionNormalX +
                             g_velocityY * collisionNormalY +
                             g_velocityZ * collisionNormalZ;

            // Reflect velocity based on collision normal
            if (dotProduct < 0) {
                g_velocityX -= (1.0f + RESTITUTION) * dotProduct * collisionNormalX;
                g_velocityY -= (1.0f + RESTITUTION) * dotProduct * collisionNormalY;
                g_velocityZ -= (1.0f + RESTITUTION) * dotProduct * collisionNormalZ;

                // Add some angular velocity change based on collision point
                // This is a simplified approach to add some rotation on impact
                g_angularVelocityX += (rand() % 100) / 1000.0f - 0.05f;
                g_angularVelocityY += (rand() % 100) / 1000.0f - 0.05f;
                g_angularVelocityZ += (rand() % 100) / 1000.0f - 0.05f;
            }
        }
    }
}



// Define unit vectors for the cube's faces (in local space)
float g_face_normals[6][3] = {
    { 0.0f,  0.0f, -1.0f},  // Back face (-Z)
    { 0.0f,  0.0f,  1.0f},  // Front face (+Z)
    { 0.0f, -1.0f,  0.0f},  // Bottom face (-Y)
    { 0.0f,  1.0f,  0.0f},  // Top face (+Y)
    {-1.0f,  0.0f,  0.0f},  // Left face (-X)
    { 1.0f,  0.0f,  0.0f}   // Right face (+X)
};

// [Keep other global variables as they are]

//*****************************************************************************
// Rotate a direction vector by the cube's current orientation
//*****************************************************************************
void RotateVector(float x, float y, float z, float* rx, float* ry, float* rz)
{
    // Similar to RotatePoint but doesn't apply translation
    // Apply X-axis rotation (pitch)
    float temp_y = y;
    float temp_z = z;
    y = temp_y * cosf(g_angleX) - temp_z * sinf(g_angleX);
    z = temp_y * sinf(g_angleX) + temp_z * cosf(g_angleX);

    // Apply Y-axis rotation (yaw)
    float temp_x = x;
    temp_z = z;
    x = temp_x * cosf(g_angleY) + temp_z * sinf(g_angleY);
    z = -temp_x * sinf(g_angleY) + temp_z * cosf(g_angleY);

    // Apply Z-axis rotation (roll)
    temp_x = x;
    temp_y = y;
    x = temp_x * cosf(g_angleZ) - temp_y * sinf(g_angleZ);
    y = temp_x * sinf(g_angleZ) + temp_y * cosf(g_angleZ);

    *rx = x;
    *ry = y;
    *rz = z;
}

//*****************************************************************************
// Calculate torque to stabilize cube onto a face
//*****************************************************************************
void CalculateStabilizingTorque(float gravityX, float gravityY, float gravityZ,
                               float* torqueX, float* torqueY, float* torqueZ)
{
    int i;
    float bestAlignment = -1.0f;  // Dot product closest to -1 (face down)
    int bestFace = -1;
    float worldNormalX, worldNormalY, worldNormalZ;

    // Find which face is most closely aligned with gravity (facing down)
    for (i = 0; i < 6; i++) {
        // Transform face normal to world space
        RotateVector(
            g_face_normals[i][0],
            g_face_normals[i][1],
            g_face_normals[i][2],
            &worldNormalX, &worldNormalY, &worldNormalZ
        );

        // Calculate dot product with gravity vector (negative means facing down)
        float dotProduct = worldNormalX * gravityX +
                          worldNormalY * gravityY +
                          worldNormalZ * gravityZ;

        // Check if this face is better aligned with gravity direction
        if (dotProduct < bestAlignment) {
            bestAlignment = dotProduct;
            bestFace = i;
        }
    }

    // If we found a "best" face and it's not perfectly aligned
    if (bestFace != -1 && bestAlignment > -0.99f) {
        // Get the world space normal of the best face
        RotateVector(
            g_face_normals[bestFace][0],
            g_face_normals[bestFace][1],
            g_face_normals[bestFace][2],
            &worldNormalX, &worldNormalY, &worldNormalZ
        );

        // Calculate cross product between face normal and gravity to get rotation axis
        float crossX = worldNormalY * gravityZ - worldNormalZ * gravityY;
        float crossY = worldNormalZ * gravityX - worldNormalX * gravityZ;
        float crossZ = worldNormalX * gravityY - worldNormalY * gravityX;

        // Normalize the cross product
        float crossLength = sqrtf(crossX * crossX + crossY * crossY + crossZ * crossZ);
        if (crossLength > 0.001f) {
            crossX /= crossLength;
            crossY /= crossLength;
            crossZ /= crossLength;

            // The magnitude of rotation should be proportional to the misalignment
            float rotationMagnitude = acosf(-bestAlignment) * STABILIZATION_STRENGTH;

            // Set the torque
            *torqueX = crossX * rotationMagnitude;
            *torqueY = crossY * rotationMagnitude;
            *torqueZ = crossZ * rotationMagnitude;
        } else {
            // If cross product is zero, no torque needed
            *torqueX = 0.0f;
            *torqueY = 0.0f;
            *torqueZ = 0.0f;
        }
    } else {
        // If already aligned or no best face, no torque needed
        *torqueX = 0.0f;
        *torqueY = 0.0f;
        *torqueZ = 0.0f;
    }
}

//*****************************************************************************
// Update physics simulation
//*****************************************************************************
void UpdatePhysics()
{
    // 1. The accelerometer readings directly define the gravity direction
    // Normalize accelerometer readings to create a gravity direction vector
    float gravityX = g_iAccelY;  // Flip sign if needed based on orientation
    float gravityY = -g_iAccelZ;  // Flip sign if needed based on orientation
    float gravityZ = -g_iAccelX;  // Flip sign if needed based on orientation

    // Calculate magnitude of the gravity vector
    float gravityMagnitude = sqrtf(gravityX * gravityX +
                                 gravityY * gravityY +
                                 gravityZ * gravityZ);

    // Prevent division by zero
    if (gravityMagnitude > 0.001f) {
        // Normalize gravity vector
        gravityX /= gravityMagnitude;
        gravityY /= gravityMagnitude;
        gravityZ /= gravityMagnitude;

        // Apply gravity to velocity (scaled by gravity strength)
        g_velocityX += gravityX * GRAVITY_STRENGTH * TIME_STEP;
        g_velocityY += gravityY * GRAVITY_STRENGTH * TIME_STEP;
        g_velocityZ += gravityZ * GRAVITY_STRENGTH * TIME_STEP;

        // 2. Calculate stabilizing torque to align cube with gravity
        float torqueX, torqueY, torqueZ;
        CalculateStabilizingTorque(gravityX, gravityY, gravityZ, &torqueX, &torqueY, &torqueZ);

        // Apply stabilizing torque to angular velocity
        g_angularVelocityX += torqueX;
        g_angularVelocityY += torqueY;
        g_angularVelocityZ += torqueZ;
    }

    // Apply damping to velocities (simulates air resistance/friction)
    g_velocityX *= DAMPING;
    g_velocityY *= DAMPING;
    g_velocityZ *= DAMPING;

    // 3. Update position based on velocity
    g_positionX += g_velocityX * TIME_STEP;
    g_positionY += g_velocityY * TIME_STEP;
    g_positionZ += g_velocityZ * TIME_STEP;

    // 4. Update angular velocity with damping
    g_angularVelocityX *= ANGULAR_DAMPING;
    g_angularVelocityY *= ANGULAR_DAMPING;
    g_angularVelocityZ *= ANGULAR_DAMPING;

    // 5. Update orientation based on angular velocity
    g_angleX += g_angularVelocityX * TIME_STEP;
    g_angleY += g_angularVelocityY * TIME_STEP;
    g_angleZ += g_angularVelocityZ * TIME_STEP;

    // 6. Collision detection and response with environment boundaries
    CheckAndResolveCollisions();
}

//*****************************************************************************
// Render the environment bounding walls as wireframe with optimized line drawing
//*****************************************************************************
void RenderEnvironment(uint16_t color)
{
    int i;

    // Calculate projected vertices for environment walls (fixed in world space)
    for (i = 0; i < NUM_VERTICES; i++) {
        ProjectEnvironmentPoint(
            g_environment_vertices[i][0],  // Already centered around origin
            g_environment_vertices[i][1],
            g_environment_vertices[i][2],
            &g_projected_env_vertices[i][0],
            &g_projected_env_vertices[i][1]
        );
    }

    // If not the first frame, erase the previous environment frame
    if (!g_first_frame) {
        // Draw black lines over previous environment edges
        for (i = 0; i < NUM_EDGES; i++) {
            int v1 = g_cube_edges[i][0];  // Reuse cube edge topology
            int v2 = g_cube_edges[i][1];

            int x1 = g_prev_projected_env_vertices[v1][0];
            int y1 = g_prev_projected_env_vertices[v1][1];
            int x2 = g_prev_projected_env_vertices[v2][0];
            int y2 = g_prev_projected_env_vertices[v2][1];

            // Skip lines that are suspiciously long (likely projection errors)
            int dx = abs(x2 - x1);
            int dy = abs(y2 - y1);
            if (dx > SCREEN_WIDTH || dy > SCREEN_HEIGHT) {
                continue; // Skip this line
            }

            // Use optimized functions for horizontal/vertical lines
            if (y1 == y2 && dx > 0) {
                // Horizontal line
                int startX = (x1 < x2) ? x1 : x2;
                int width = dx + 1;
                drawFastHLine(startX, y1, width, BLACK);
            }
            else if (x1 == x2 && dy > 0) {
                // Vertical line
                int startY = (y1 < y2) ? y1 : y2;
                int height = dy + 1;
                drawFastVLine(x1, startY, height, BLACK);
            }
            else if (dx > 0 || dy > 0) {
                // Diagonal line - use regular line function
                drawLine(x1, y1, x2, y2, BLACK);
            }
            // Skip zero-length lines
        }
    }

    // Draw new environment edges with optimized line functions
    for (i = 0; i < NUM_EDGES; i++) {
        int v1 = g_cube_edges[i][0];  // Reuse cube edge topology
        int v2 = g_cube_edges[i][1];

        int x1 = g_projected_env_vertices[v1][0];
        int y1 = g_projected_env_vertices[v1][1];
        int x2 = g_projected_env_vertices[v2][0];
        int y2 = g_projected_env_vertices[v2][1];

        // Skip lines that are suspiciously long (likely projection errors)
        int dx = abs(x2 - x1);
        int dy = abs(y2 - y1);
        if (dx > SCREEN_WIDTH || dy > SCREEN_HEIGHT) {
            continue; // Skip this line
        }

        // Use optimized functions for horizontal/vertical lines
        if (y1 == y2 && dx > 0) {
            // Horizontal line
            int startX = (x1 < x2) ? x1 : x2;
            int width = dx + 1;
            drawFastHLine(startX, y1, width, color);
        }
        else if (x1 == x2 && dy > 0) {
            // Vertical line
            int startY = (y1 < y2) ? y1 : y2;
            int height = dy + 1;
            drawFastVLine(x1, startY, height, color);
        }
        else if (dx > 0 || dy > 0) {
            // Diagonal line - use regular line function
            drawLine(x1, y1, x2, y2, color);
        }
        // Skip zero-length lines
    }

    // Copy current environment vertices to previous for next frame
    for (i = 0; i < NUM_VERTICES; i++) {
        g_prev_projected_env_vertices[i][0] = g_projected_env_vertices[i][0];
        g_prev_projected_env_vertices[i][1] = g_projected_env_vertices[i][1];
    }
}

//*****************************************************************************
// Render the 3D cube - MODIFIED to erase previous frame
//*****************************************************************************
void RenderCube(uint16_t color)
{
    int i;
    float rx, ry, rz;

    // Calculate projected vertices for current frame
    for (i = 0; i < NUM_VERTICES; i++) {
        // Apply rotation
        RotatePoint(
            g_cube_vertices[i][0],
            g_cube_vertices[i][1],
            g_cube_vertices[i][2],
            &rx, &ry, &rz
        );

        // Project to 2D
        ProjectPoint(rx, ry, rz,
                    &g_projected_vertices[i][0],
                    &g_projected_vertices[i][1]);
    }

    // If not the first frame, erase the previous frame by drawing black lines
    if (!g_first_frame) {
        // Draw black lines over previous edges
        for (i = 0; i < NUM_EDGES; i++) {
            int v1 = g_cube_edges[i][0];
            int v2 = g_cube_edges[i][1];

            drawLine(
                g_prev_projected_vertices[v1][0], g_prev_projected_vertices[v1][1],
                g_prev_projected_vertices[v2][0], g_prev_projected_vertices[v2][1],
                BLACK
            );
        }
    } else {
        g_first_frame = false; // No longer the first frame
    }

    // Draw new edges with the specified color
    for (i = 0; i < NUM_EDGES; i++) {
        int v1 = g_cube_edges[i][0];
        int v2 = g_cube_edges[i][1];

        drawLine(
            g_projected_vertices[v1][0], g_projected_vertices[v1][1],
            g_projected_vertices[v2][0], g_projected_vertices[v2][1],
            color
        );
    }

    // Copy current vertices to previous vertices for the next frame
    for (i = 0; i < NUM_VERTICES; i++) {
        g_prev_projected_vertices[i][0] = g_projected_vertices[i][0];
        g_prev_projected_vertices[i][1] = g_projected_vertices[i][1];
    }
}

//*****************************************************************************
// Board Initialization & Configuration
//*****************************************************************************
static void BoardInit(void)
{
#ifndef USE_TIRTOS
    // Set vector table base
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    // Enable Processor
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}

//*****************************************************************************
// Main function
//*****************************************************************************
// Initialize the 3D cube application
void Cube3D_Initialize(void)
{
    // Display banner message
    UART_PRINT("Starting 3D Cube with Accelerometer Control and Physics...\n\r");


    MAP_PinTypeI2C(PIN_01, PIN_MODE_1);

   //
   // Configure PIN_02 for I2C0 I2C_SDA
   //
    MAP_PinTypeI2C(PIN_02, PIN_MODE_1);


    // Open I2C interface for accelerometer
    I2C_IF_Open(I2C_MASTER_MODE_FST);

    // Clear the screen
    fillScreen(BLACK);

    // Initialize first frame flag
    g_first_frame = true;

    // Initialize random number generator for physics effects
    srand(1234);  // Fixed seed for reproducible results
}

// Run one frame of the 3D cube application
// Returns: true to continue running, false to exit
bool Cube3D_RunFrame(void)
{
    // Check if button 2 is pressed to exit
    if(Cube3D_ShouldExit()) {
        return false;
    }

    if(ReadAccelerometerData() == SUCCESS)
    {
        // Update physics (accelerometer data is now read inside UpdatePhysics)
        UpdatePhysics();

        // Render the environment walls first (so cube appears on top)
        RenderEnvironment(WALL_COLOR);

        // Render the cube with the updated position and rotation
        RenderCube(WHITE);

        // Small delay between frames
        MAP_UtilsDelay(80000);
    }
    else
    {
        UART_PRINT("Error reading accelerometer!\n\r");
        // Add delay before retrying
        MAP_UtilsDelay(800000);
    }

    return true;
}

// Clean up resources before exiting
void Cube3D_Cleanup(void)
{
    // Clear the screen
    fillScreen(BLACK);

    // Reset any state if needed
    g_first_frame = true;
}
