/******************************************************************************
 * TI CC3200 Multi-Application OS
 * Main selection screen for moving between several demos
 * Based on ADC joystick, GPIO buttons, and SSD1351 OLED
 *
 * Version: 1.0.0
 * Target: TI CC3200
 * Standard: C90
 ******************************************************************************/

/*============================================================================
 * SYSTEM INCLUDES
 *============================================================================*/
/* Hardware abstraction includes */
#include "hw_memmap.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_adc.h"
#include "hw_wdt.h"

/* ROM and peripheral includes */
#include "rom.h"
#include "rom_map.h"
#include "pin.h"
#include "adc.h"
#include "prcm.h"
#include "wdt.h"

/* Application includes */
#include <shared_defs.h>
#include "uart_if.h"
#include "i2c_if.h"
#include "pinmux.h"

/*============================================================================
 * APPLICATION INCLUDES
 *============================================================================*/
#include "sound_Effects.h"
#include "cube3d.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "cursor_bitmap.h"
#include "optionBackground_bitmap.h"
#include "INTRO.h"
#include "video_game.h"
#include "AWS_IoT.h"
#include "functiongenerator.h"

/*============================================================================
 * CONSTANTS AND DEFINITIONS
 *============================================================================*/
/* Application constants */
#define APPLICATION_VERSION     "1.0.0"
#define APP_NAME                "TI OS"
#define SPI_IF_BIT_RATE         20000000

/* System constants */
#define FOREVER                 1
#define SUCCESS                 0
#define FAILURE                 -1
#define CONSOLE                 UARTA0_BASE

/* Display constants */
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           128
#define SCREEN_CENTER_X         (SCREEN_WIDTH / 2)
#define SCREEN_CENTER_Y         (SCREEN_HEIGHT / 2)

/* Button constants */
#define BUTTON1_PIN             0x40    /* PIN_15 */
#define BUTTON1_PORT            GPIOA2_BASE
#define BUTTON1_INT             INT_GPIOA2

#define BUTTON2_PIN             0x20    /* PIN_21 */
#define BUTTON2_PORT            GPIOA1_BASE
#define BUTTON2_INT             INT_GPIOA1

/* Timing constants */
#define SYSTICK_WRAP_TICKS      16777216
#define DEBOUNCE_DELAY          0
#define SYSTEM_CLOCK_FREQ       80000000
#define TICKS_TO_MS(ticks)      ((ticks) / (SYSTEM_CLOCK_FREQ / 1000))

/* Accelerometer constants */
#define ACCEL_I2C_ADDR          0x18
#define ACCEL_REG_X             0x03
#define ACCEL_REG_Y             0x05
#define ACCEL_REG_Z             0x07

/* ADC constants */
#define ADC_SAMPLE_COUNT        10
#define ADC_REFERENCE_VOLTAGE   1.4f
#define ADC_RESOLUTION_BITS     12
#define ADC_MAX_VALUE           4096

/* Joystick constants */
#define JOYSTICK_CENTER_VOLTAGE 0.7f
#define JOYSTICK_DEADZONE       0.1f
#define DEFAULT_CURSOR_SENSITIVITY 20

/*============================================================================
 * MACROS
 *============================================================================*/
#define UART_PRINT              Report
#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}

/*============================================================================
 * TYPE DEFINITIONS
 *============================================================================*/
typedef struct {
    unsigned long tickValue;
    unsigned long wrapCount;
} SystemTime;

typedef struct {
    float cursorx;
    float cursory;
    float lastcursorx;
    float lastcursory;
    bool joystickMoved;
    bool buttonPressed;
    bool hideCursor;
    int cursorFrame;
    int introFrame;
    int optionBackgroundFrame;
    int cursorSensitivity;
    char* currentInterface;
    char previousSelectedOption;
    char selectedOption;
    int animationDelay;
    bool firstIntroFrame;
} GameState;

/*============================================================================
 * GLOBAL VARIABLES
 *============================================================================*/
/* Interrupt vector table */
#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

/* Accelerometer data */
int g_iAccelX = 0;
int g_iAccelY = 0;
int g_iAccelZ = 0;

/* ADC voltage readings */
float voltage_59 = 0.0;
float voltage_60 = 0.0;

/* Button state variables */
static volatile bool buttonEvent = false;
static volatile bool buttonCurrentState = false;
static volatile bool button2Event = false;
static volatile bool button2State = false;
static volatile bool button1Pressed = false;
static volatile bool button2Pressed = false;
static volatile bool buttonStateChanged = false;
static volatile bool buttonHeld = false;
static volatile int currentButton = 0;
static volatile bool screenNeedsUpdate = false;

/* System timing */
static volatile unsigned long sysTickWrapCounter = 0;

/* Application state */
static bool videogameInitialized = false;

/*============================================================================
 * FUNCTION PROTOTYPES
 *============================================================================*/
/* System initialization */
static void BoardInit(void);
static void DisplayBanner(char* AppName);
void init_simplelink(void);

/* Hardware interface functions */
int ReadAccelerometerData(void);
static void ReadADCChannel(unsigned int uiChannel, float *voltageResult);

/* Button handling */
static void InitializeBothButtons(void);
static void ButtonHandler(void);
static void Button2Handler(void);
static void ButtonEnableInterrupt(void);

/* Timing functions */
static void TimingReset(void);
static unsigned long CalculateTimeDifference(SystemTime startTime, SystemTime endTime);

/* Game state management */
void initializeHardware(void);
void initializeGameState(GameState* state);
void processInput(GameState* state);
void updateGameLogic(GameState* state);
void renderInterface(GameState* state);

/* UI and rendering */
void handleButtonPress(GameState* state);
void renderIntroScreen(GameState* state);
void renderOptionScreen(GameState* state);
void renderApplication(GameState* state, uint16_t color);
void updateCursorPosition(GameState* state);
bool checkHitbox(float x, float y, int x1, int y1, int x2, int y2);

/*============================================================================
 * SYSTEM INITIALIZATION FUNCTIONS
 *============================================================================*/

/**
 * Initialize board peripherals and system settings
 */
static void BoardInit(void)
{
    /* Enable watchdog timer */
    MAP_PRCMPeripheralClkEnable(PRCM_WDT, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_WDT);

    /* Setup interrupt vector table */
#ifndef USE_TIRTOS
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif

    /* Enable interrupts */
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);
    PRCMCC3200MCUInit();
}

/**
 * Display application banner
 */
static void DisplayBanner(char* AppName)
{
    Report("\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t      CC3200 %s Application       \n\r", AppName);
    Report("\t\t *************************************************\n\r");
    Report("\n\n\n\r");
}

/**
 * Initialize SimpleLink for file operations
 */
void init_simplelink(void)
{
    int mode;

    UART_PRINT("Initializing SimpleLink...\n\r");
    UART_PRINT("Attempting to start SimpleLink\n\r");

    mode = sl_Start(NULL, NULL, NULL);
    if(mode < 0) {
        UART_PRINT("Error: SimpleLink failed to start (%d)\n\r", mode);
        return;
    }

    UART_PRINT("SimpleLink started in mode: %d\n\r", mode);

    /* Configure for station mode */
    if(mode != ROLE_STA) {
        sl_WlanSetMode(ROLE_STA);
        sl_Stop(0xFF);
        sl_Start(NULL, NULL, NULL);
    }

    /* Disable connection policy */
    sl_WlanPolicySet(SL_POLICY_CONNECTION, SL_CONNECTION_POLICY(0, 0, 0, 0, 0), NULL, 0);

    UART_PRINT("SimpleLink initialized for file operations\n\r");
}

/*============================================================================
 * HARDWARE INTERFACE FUNCTIONS
 *============================================================================*/

/**
 * Read accelerometer data from BMA222
 * @return SUCCESS on success, FAILURE on error
 */
int ReadAccelerometerData(void)
{
    unsigned char ucDevAddr = ACCEL_I2C_ADDR;
    unsigned char ucRegX = ACCEL_REG_X;
    unsigned char ucRegY = ACCEL_REG_Y;
    unsigned char ucRegZ = ACCEL_REG_Z;
    unsigned char ucData = 0;

    /* Read X-axis */
    if(I2C_IF_Write(ucDevAddr, &ucRegX, 1, 0) != SUCCESS) return FAILURE;
    if(I2C_IF_Read(ucDevAddr, &ucData, 1) != SUCCESS) return FAILURE;
    g_iAccelX = (int8_t)ucData;

    /* Read Y-axis */
    if(I2C_IF_Write(ucDevAddr, &ucRegY, 1, 0) != SUCCESS) return FAILURE;
    if(I2C_IF_Read(ucDevAddr, &ucData, 1) != SUCCESS) return FAILURE;
    g_iAccelY = (int8_t)ucData;

    /* Read Z-axis */
    if(I2C_IF_Write(ucDevAddr, &ucRegZ, 1, 0) != SUCCESS) return FAILURE;
    if(I2C_IF_Read(ucDevAddr, &ucData, 1) != SUCCESS) return FAILURE;
    g_iAccelZ = (int8_t)ucData;

    return SUCCESS;
}

/**
 * Read ADC channel and calculate voltage
 * @param uiChannel ADC channel to read
 * @param voltageResult Pointer to store voltage result
 */
static void ReadADCChannel(unsigned int uiChannel, float *voltageResult)
{
    unsigned int uiIndex = 0;
    unsigned long ulSample;
    float avgVoltage = 0.0f;

    /* Enable ADC channel */
    MAP_ADCChannelEnable(ADC_BASE, uiChannel);

    /* Sample multiple times and average */
    while(uiIndex < ADC_SAMPLE_COUNT)
    {
        if(MAP_ADCFIFOLvlGet(ADC_BASE, uiChannel))
        {
            ulSample = MAP_ADCFIFORead(ADC_BASE, uiChannel);
            /* Calculate voltage */
            avgVoltage += (((float)((ulSample >> 2) & 0x0FFF)) * ADC_REFERENCE_VOLTAGE) / ADC_MAX_VALUE;
            uiIndex++;
        }
    }

    /* Disable ADC channel */
    MAP_ADCChannelDisable(ADC_BASE, uiChannel);

    /* Return average voltage */
    *voltageResult = avgVoltage / ADC_SAMPLE_COUNT;
}

/*============================================================================
 * BUTTON HANDLING FUNCTIONS
 *============================================================================*/

/**
 * Initialize both button interrupts
 */
static void InitializeBothButtons(void)
{
    /* Enable GPIO peripheral clocks */
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA2, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA3, PRCM_RUN_MODE_CLK);

    /* Configure Button 1 (PIN_15) */
    MAP_GPIOIntTypeSet(BUTTON1_PORT, BUTTON1_PIN, GPIO_BOTH_EDGES);
    MAP_GPIOIntRegister(BUTTON1_PORT, ButtonHandler);
    MAP_IntPrioritySet(BUTTON1_INT, INT_PRIORITY_LVL_3);
    MAP_GPIOIntClear(BUTTON1_PORT, BUTTON1_PIN);
    MAP_GPIOIntEnable(BUTTON1_PORT, BUTTON1_PIN);

    /* Configure Button 2 (PIN_17) */
    MAP_GPIOIntTypeSet(BUTTON2_PORT, BUTTON2_PIN, GPIO_BOTH_EDGES);
    MAP_GPIOIntRegister(BUTTON2_PORT, Button2Handler);
    MAP_IntPrioritySet(BUTTON2_INT, INT_PRIORITY_LVL_3);
    MAP_GPIOIntClear(BUTTON2_PORT, BUTTON2_PIN);
    MAP_GPIOIntEnable(BUTTON2_PORT, BUTTON2_PIN);

    /* Enable interrupt controllers */
    MAP_IntEnable(BUTTON1_INT);
    MAP_IntEnable(BUTTON2_INT);
}

/**
 * Button 1 interrupt handler
 */
static void ButtonHandler(void)
{
    unsigned long ulStatus;
    bool button1State, button2State;

    /* Clear the interrupt */
    ulStatus = GPIOIntStatus(BUTTON1_PORT, true);
    GPIOIntClear(BUTTON1_PORT, ulStatus);

    /* Read current button states (active low) */
    button1State = !(GPIOPinRead(BUTTON1_PORT, BUTTON1_PIN) == 0);
    button2State = !(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0);

    /* Update global state */
    if (button1State) {
        buttonHeld = true;
        currentButton = 1;
        screenNeedsUpdate = true;
    } else if (button2State) {
        buttonHeld = true;
        currentButton = 2;
        screenNeedsUpdate = true;
    } else {
        buttonHeld = false;
        currentButton = 0;
        screenNeedsUpdate = true;
    }
}

/**
 * Button 2 interrupt handler
 */
static void Button2Handler(void)
{
    unsigned long ulStatus;
    bool button1State, button2State;

    /* Clear the interrupt */
    ulStatus = GPIOIntStatus(BUTTON2_PORT, true);
    GPIOIntClear(BUTTON2_PORT, ulStatus);

    /* Read current button states (active low) */
    button2State = !(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0);
    button1State = !(GPIOPinRead(BUTTON1_PORT, BUTTON1_PIN) == 0);

    /* Update global state */
    if (button2State) {
        buttonHeld = true;
        currentButton = 2;
        screenNeedsUpdate = true;
    } else if (button1State) {
        buttonHeld = true;
        currentButton = 1;
        screenNeedsUpdate = true;
    } else {
        buttonHeld = false;
        currentButton = 0;
        screenNeedsUpdate = true;
    }
}

/**
 * Enable button interrupts
 */
static void ButtonEnableInterrupt(void)
{
    /* Enable Button 1 interrupt */
    MAP_GPIOIntClear(BUTTON1_PORT, BUTTON1_PIN);
    MAP_IntPendClear(BUTTON1_INT);
    MAP_IntEnable(BUTTON1_INT);
    MAP_GPIOIntEnable(BUTTON1_PORT, BUTTON1_PIN);

    /* Enable Button 2 interrupt */
    MAP_GPIOIntClear(BUTTON2_PORT, BUTTON2_PIN);
    MAP_IntPendClear(BUTTON2_INT);
    MAP_IntEnable(BUTTON2_INT);
    MAP_GPIOIntEnable(BUTTON2_PORT, BUTTON2_PIN);
}

/*============================================================================
 * APPLICATION INITIALIZATION FUNCTIONS
 *============================================================================*/

/**
 * Initialize all hardware components
 */
void initializeHardware(void)
{
    /* Initialize board and peripherals */
    BoardInit();
    PinMuxConfig();
    InitTerm();

    /* Enable SPI module clock */
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);

    /* Enable GPIO clocks */
    MAP_PRCMPeripheralClkEnable(PRCM_GPIOA0, PRCM_RUN_MODE_CLK);

    /* Configure DC and RESET pins as outputs */
    MAP_GPIODirModeSet(GPIOA0_BASE, 0x40, GPIO_DIR_MODE_OUT);  /* DC pin */
    MAP_GPIODirModeSet(GPIOA0_BASE, 0x80, GPIO_DIR_MODE_OUT);  /* RESET pin */

    /* Configure SPI interface */
    MAP_SPIReset(GSPI_BASE);
    MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                         SPI_IF_BIT_RATE, SPI_MODE_MASTER, SPI_SUB_MODE_0,
                         (SPI_SW_CTRL_CS | SPI_4PIN_MODE | SPI_TURBO_OFF |
                          SPI_CS_ACTIVEHIGH | SPI_WL_8));

    /* Enable SPI and initialize display */
    MAP_SPIEnable(GSPI_BASE);
    Adafruit_Init();

    /* Initialize buttons and sound */
    InitializeBothButtons();
    InitSoundEffects();
    ButtonEnableInterrupt();

    /* Initialize ADC for joystick */
#ifdef CC3200_ES_1_2_1
    HWREG(GPRCM_BASE + GPRCM_O_ADC_CLK_CONFIG) = 0x00000043;
    HWREG(ADC_BASE + ADC_O_ADC_CTRL) = 0x00000004;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE0) = 0x00000100;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE1) = 0x0355AA00;
#endif

    /* Configure ADC pins */
    MAP_PinTypeADC(PIN_58, PIN_MODE_255);
    MAP_PinTypeADC(PIN_57, PIN_MODE_255);
    MAP_PinTypeADC(PIN_59, PIN_MODE_255);
    MAP_PinTypeADC(PIN_60, PIN_MODE_255);

    /* Configure and enable ADC */
    MAP_ADCTimerConfig(ADC_BASE, 2^17);
    MAP_ADCTimerEnable(ADC_BASE);
    MAP_ADCEnable(ADC_BASE);

    /* Display startup information */
    DisplayBanner(APP_NAME);
    UART_PRINT("Starting 3D Cube with Accelerometer Control...\n\r");
    UART_PRINT("ADC initialized for continuous monitoring...\n\r");

    /* Clear display */
    fillScreen(BLACK);
}

/**
 * Initialize game state structure
 */
void initializeGameState(GameState* state)
{
    state->cursorx = 0.0f;
    state->cursory = 0.0f;
    state->lastcursorx = 0.0f;
    state->lastcursory = 0.0f;
    state->joystickMoved = false;
    state->buttonPressed = false;
    state->hideCursor = true;
    state->cursorFrame = 0;
    state->introFrame = 0;
    state->optionBackgroundFrame = 0;
    state->cursorSensitivity = DEFAULT_CURSOR_SENSITIVITY;
    state->currentInterface = "intro";
    state->previousSelectedOption = 1;
    state->selectedOption = 1;
    state->animationDelay = 800000;
    state->firstIntroFrame = true;
}

/*============================================================================
 * INPUT PROCESSING FUNCTIONS
 *============================================================================*/

/**
 * Process input from buttons and joystick
 */
void processInput(GameState* state)
{
    /* Check button events */
    state->buttonPressed = screenNeedsUpdate;
    if (screenNeedsUpdate) {
        screenNeedsUpdate = false;
        handleButtonPress(state);
    }

    /* Read joystick input via ADC */
    updateCursorPosition(state);
}

/**
 * Update cursor position based on joystick input
 */
void updateCursorPosition(GameState* state)
{
    float voltage_57, voltage_58, voltage_59, voltage_60;
    float xOffset, yOffset;

    /* Read ADC values */
    ReadADCChannel(ADC_CH_0, &voltage_57);
    ReadADCChannel(ADC_CH_1, &voltage_58);
    ReadADCChannel(ADC_CH_2, &voltage_59);
    ReadADCChannel(ADC_CH_3, &voltage_60);

    /* Calculate proportional offset from center */
    xOffset = ((voltage_59 / ADC_REFERENCE_VOLTAGE) - 0.5f);
    yOffset = ((voltage_60 / ADC_REFERENCE_VOLTAGE) - 0.5f);

    /* Apply deadzone to prevent drift */
    if(fabs(xOffset) >= JOYSTICK_DEADZONE) {
        state->cursorx -= xOffset * state->cursorSensitivity;
    }

    if(fabs(yOffset) >= JOYSTICK_DEADZONE) {
        state->cursory += yOffset * state->cursorSensitivity;
    }

    /* Apply boundary constraints */
    if (state->cursorx >= SCREEN_WIDTH - CURSOR_WIDTH) {
        state->cursorx = SCREEN_WIDTH - CURSOR_WIDTH;
    }
    if (state->cursorx <= 0) {
        state->cursorx = 0;
    }
    if (state->cursory >= SCREEN_HEIGHT - CURSOR_HEIGHT) {
        state->cursory = SCREEN_HEIGHT - CURSOR_HEIGHT;
    }
    if (state->cursory <= 0) {
        state->cursory = 0;
    }

    /* Update movement detection */
    state->joystickMoved = (state->cursorx != state->lastcursorx) ||
                          (state->cursory != state->lastcursory);
    state->lastcursorx = state->cursorx;
    state->lastcursory = state->cursory;
}

/*============================================================================
 * GAME LOGIC FUNCTIONS
 *============================================================================*/

/**
 * Handle button press events and navigation
 */
void handleButtonPress(GameState* state)
{
    if (currentButton == 1) {
        PlayButtonSound();
        Report("\n\r(Button 1)");
        state->cursorFrame = 2;

        /* Handle option selection */
        if (strcmp(state->currentInterface, "optionScreen") == 0) {
            switch (state->selectedOption) {
                case 1:
                    fastFillScreen(BLACK);
                    state->currentInterface = "Function Generator";
                    state->hideCursor = true;
                    FunctionGenerator_Initialize();
                    break;
                case 2:
                    fastFillScreen(BLACK);
                    state->currentInterface = "Oscilliscope";
                    state->hideCursor = true;
                    Oscilloscope_Initialize();
                    break;
                case 3:
                    fastFillScreen(BLACK);
                    state->currentInterface = "AWS IoT";
                    state->hideCursor = true;
                    AWSIoT_Initialize();
                    break;
                case 4:
                    fastFillScreen(BLACK);
                    state->currentInterface = "Video Game";
                    state->hideCursor = true;
                    VideoGame_Initialize();
                    PlayThemeSoundLooped();
                    break;
                case 5:
                    fastFillScreen(BLACK);
                    state->currentInterface = "3D Cube";
                    state->hideCursor = true;
                    Cube3D_Initialize();
                    break;
                case 6:
                    fastFillScreen(BLACK);
                    state->currentInterface = "Servo Control";
                    state->hideCursor = true;
                    ServoControl_Initialize();
                    break;
                default:
                    state->currentInterface = "optionScreen";
                    break;
            }
        }
    } else if (currentButton == 2) {
        PlaySuccessSound();
        Report("\n\rScreen: BLUE (Button 2)");

        /* Return to option screen from applications */
        if (strcmp(state->currentInterface, "3D Cube") == 0 ||
            strcmp(state->currentInterface, "Video Game") == 0 ||
            strcmp(state->currentInterface, "Servo Control") == 0 ||
            strcmp(state->currentInterface, "Oscilliscope") == 0) {

            if (strcmp(state->currentInterface, "3D Cube") == 0) {
                Cube3D_Cleanup();
            }

            state->currentInterface = "optionScreen";
            state->hideCursor = false;
        }
    } else {
        Report("\n\rScreen: BLACK (No buttons)");
        state->cursorFrame = 0;
    }
}

/**
 * Update game logic and selection based on cursor position
 */
void updateGameLogic(GameState* state)
{
    /* Handle hitbox detection for menu selections */
    if(strcmp(state->currentInterface, "optionScreen") == 0) {
        if (checkHitbox(state->cursorx, state->cursory, 0, 0, 21, 22)) {
            state->selectedOption = 1;
            state->optionBackgroundFrame = 0;
            state->cursorFrame = 1;
        } else if (checkHitbox(state->cursorx, state->cursory, 0, 25, 21, 43)) {
            state->selectedOption = 2;
            state->optionBackgroundFrame = 1;
            state->cursorFrame = 1;
        } else if (checkHitbox(state->cursorx, state->cursory, 0, 46, 21, 64)) {
            state->selectedOption = 3;
            state->optionBackgroundFrame = 2;
            state->cursorFrame = 1;
        } else if (checkHitbox(state->cursorx, state->cursory, 107, 0, 128, 21)) {
            state->selectedOption = 4;
            state->optionBackgroundFrame = 3;
            state->cursorFrame = 1;
        } else if (checkHitbox(state->cursorx, state->cursory, 107, 25, 128, 43)) {
            state->selectedOption = 5;
            state->optionBackgroundFrame = 4;
            state->cursorFrame = 1;
        } else if (checkHitbox(state->cursorx, state->cursory, 107, 46, 128, 64)) {
            state->selectedOption = 6;
            state->optionBackgroundFrame = 5;
            state->cursorFrame = 1;
        } else if (checkHitbox(state->cursorx, state->cursory, 0, 119, 10, 128)) {
            state->selectedOption = 7;
            state->optionBackgroundFrame = 6;
            state->cursorFrame = 1;
        } else {
            state->cursorFrame = 0;
            state->optionBackgroundFrame = 7;
            state->selectedOption = 0;
        }

        /* Play sound when selection changes */
        if (state->previousSelectedOption != state->selectedOption) {
            PlayClickSound();
        }

        state->previousSelectedOption = state->selectedOption;
    }
}

/*============================================================================
 * RENDERING FUNCTIONS
 *============================================================================*/

/**
 * Main rendering function - coordinates all display updates
 */
void renderInterface(GameState* state)
{
    /* Handle function generator background operation */
    if((strcmp(state->currentInterface, "Function Generator") != 0) &&
       FunctionGenerator_IsEnabled()) {
        FunctionGenerator_PlayFrequency();
    }

    /* Render OS-style interfaces when input changes */
    if (buttonHeld || state->joystickMoved || state->buttonPressed ||
        !(strcmp(state->currentInterface, "intro"))) {

        if (strcmp(state->currentInterface, "intro") == 0) {
            renderIntroScreen(state);
        }
        if (strcmp(state->currentInterface, "optionScreen") == 0) {
            renderOptionScreen(state);
            StopThemeLoop();
        }
    }

    /* Render applications that need continuous updates */
    if (strcmp(state->currentInterface, "3D Cube") == 0 ||
        strcmp(state->currentInterface, "Video Game") == 0 ||
        strcmp(state->currentInterface, "Servo Control") == 0 ||
        strcmp(state->currentInterface, "Oscilliscope") == 0 ||
        strcmp(state->currentInterface, "AWS IoT") == 0 ||
        strcmp(state->currentInterface, "Function Generator") == 0) {
        renderApplication(state);
    }

    /* Draw cursor if not hidden */
    if (!state->hideCursor) {
        const uint8_t* cursor_frame_bitmap = get_cursor_frame(state->cursorFrame);
        drawBitmap(state->cursorx, state->cursory, cursor_frame_bitmap,
                  CURSOR_WIDTH, CURSOR_HEIGHT, WHITE, 1, false, BLACK);
    }
}

/**
 * Render the intro animation screen
 */
void renderIntroScreen(GameState* state)
{
    const uint8_t* INTRO_frame_bitmap;

    INTRO_frame_bitmap = get_INTRO_frame((state->introFrame) % INTRO_FRAME_COUNT);
    fastDrawBitmap(0, 0, INTRO_frame_bitmap, 128, 128, GREEN, BLACK, 1);
    state->introFrame++;
    MAP_UtilsDelay(state->animationDelay);

    if(state->firstIntroFrame) {
        PlayIntroSound();
        state->firstIntroFrame = false;
    }

    if (state->introFrame >= INTRO_FRAME_COUNT) {
        state->currentInterface = "optionScreen";
        state->hideCursor = false;
    }
}

/**
 * Render the options menu screen
 */
void renderOptionScreen(GameState* state)
{
    const uint8_t* optionBackground_frame_bitmap;

    optionBackground_frame_bitmap = get_optionBackground_frame(state->optionBackgroundFrame);
    fastDrawBitmap(0, 0, optionBackground_frame_bitmap, OPTIONBACKGROUND_WIDTH,
                  OPTIONBACKGROUND_HEIGHT, GREEN, BLACK, 1);
}

/**
 * Render individual applications
 */
void renderApplication(GameState* state, uint16_t color)
{
    bool button1State, button2State;

    /* Read current button states */
    button1State = !(GPIOPinRead(BUTTON1_PORT, BUTTON1_PIN) == 0);
    button2State = !(GPIOPinRead(BUTTON2_PORT, BUTTON2_PIN) == 0);

    if (strcmp(state->currentInterface, "3D Cube") == 0) {
        if (button2State) {
            Cube3D_Cleanup();
            state->currentInterface = "optionScreen";
            state->hideCursor = false;
            screenNeedsUpdate = false;
        } else {
            Cube3D_RunFrame();
        }
    } else if (strcmp(state->currentInterface, "Video Game") == 0) {
        if (!VideoGame_RunFrame()) {
            if (videogameInitialized) {
                videogameInitialized = false;
            }
            state->currentInterface = "optionScreen";
            state->hideCursor = false;
            screenNeedsUpdate = false;
        } else {
            if (!videogameInitialized) {
                VideoGame_Initialize();
                UART_PRINT("\r\nInitializing video game\n");
                videogameInitialized = true;
            }
        }
    } else if (strcmp(state->currentInterface, "Servo Control") == 0) {
        if (button2State) {
            ServoControl_Cleanup();
            state->currentInterface = "optionScreen";
            state->hideCursor = false;
            screenNeedsUpdate = false;
        } else {
            if (!ServoControl_RunFrame()) {
                ServoControl_Cleanup();
                state->currentInterface = "optionScreen";
                state->hideCursor = false;
            }
        }
    } else if (strcmp(state->currentInterface, "Oscilliscope") == 0) {
        if (button2State) {
            state->currentInterface = "optionScreen";
            state->hideCursor = false;
            screenNeedsUpdate = false;
        } else {
            if (!Oscilloscope_RunFrame()) {
                Oscilloscope_Cleanup();
                state->currentInterface = "optionScreen";
                state->hideCursor = false;
            }
        }
    } else if (strcmp(state->currentInterface, "AWS IoT") == 0) {
        if (button2State && button1State) {
            AWSIoT_Cleanup();
            state->currentInterface = "optionScreen";
            state->hideCursor = false;
            screenNeedsUpdate = false;
        } else {
            if (!AWSIoT_RunFrame()) {
                AWSIoT_Cleanup();
                state->currentInterface = "optionScreen";
                state->hideCursor = false;
            }
        }
    } else if (strcmp(state->currentInterface, "Function Generator") == 0) {
        if (button2State) {
            FunctionGenerator_Cleanup();
            state->currentInterface = "optionScreen";
            state->hideCursor = false;
            screenNeedsUpdate = false;
        } else {
            if (!FunctionGenerator_RunFrame()) {
                FunctionGenerator_Cleanup();
                state->currentInterface = "optionScreen";
                state->hideCursor = false;
            }
        }
    }
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * Check if a point is within a rectangular hitbox
 */
bool checkHitbox(float x, float y, int x1, int y1, int x2, int y2)
{
    return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
}

/*============================================================================
 * MAIN FUNCTION
 *============================================================================*/

/**
 * Main application entry point
 */
void main(void)
{
    GameState gameState;

    /* Initialize hardware components */
    initializeHardware();

    /* Initialize SimpleLink */
    init_simplelink();

    /* Initialize game state */
    initializeGameState(&gameState);

    /* Main application loop */
    while (FOREVER) {
        /* Update sound effects */
        UpdateSoundEffects();

        /* Process input */
        processInput(&gameState);

        /* Update game logic */
        updateGameLogic(&gameState);

        /* Render the current interface */
        renderInterface(&gameState);
    }
}
