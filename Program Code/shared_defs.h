#ifndef SHARED_DEFS_H
#define SHARED_DEFS_H

#include <stdbool.h>
#include <stdint.h>

// Driverlib includes - same as in your original code
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "uart.h"
#include "gpio.h"
#include "glcdfont.h"
#include "spi.h"

#include "cube3d.h"
#include "math.h"
// Common interface includes
#include "uart_if.h"
#include "i2c_if.h"
#include "pinmux.h"

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"

#include "oled_test.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "servo_control.h"
#include "oscilliscope.h"

// Shared accelerometer variables
extern int g_iAccelX;
extern int g_iAccelY;
extern int g_iAccelZ;


#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

// Shared accelerometer function declaration
int ReadAccelerometerData(void);

#endif // SHARED_DEFS_H
