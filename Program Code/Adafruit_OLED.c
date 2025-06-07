
// Standard includes
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "gpio.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"

// Common interface includes
#include "uart_if.h"
#include "pinmux.h"

#include "Adafruit_SSD1351.h"

// flush buffer variable
static unsigned long flush;



void writeCommand(unsigned char c) {
    GPIOPinWrite(GPIOA3_BASE, 0x10, 0x00); //Set DC pin low to indicate incoming command

    MAP_SPICSEnable(GSPI_BASE); // Enables CS line for peripheral to select OLED

    GPIOPinWrite(GPIOA1_BASE, 0x80, 0x00); // Sets OLEDCS low to select OLED

    MAP_SPIDataPut(GSPI_BASE, c); //Sends command byte to OLED

    MAP_SPIDataGet(GSPI_BASE, &flush); //Reads back a dummy byte to complete transmission (I used claude to figure out this was needed)

    GPIOPinWrite(GPIOA1_BASE, 0x80, 0xff); // Sets OLEDCS high, deselect OLED

    MAP_SPICSDisable(GSPI_BASE); //Disables CS line for unselecting OLED

}

void writeData(unsigned char c) {
    GPIOPinWrite(GPIOA3_BASE, 0x10, 0xff); //Set DC pin high to indicate incoming data (I used claude to figure out this was needed)

    MAP_SPICSEnable(GSPI_BASE); // Enables CS line for peripheral to select OLED

    GPIOPinWrite(GPIOA1_BASE, 0x80, 0x00); // Sets OLEDCS low to select OLED

    MAP_SPIDataPut(GSPI_BASE, c); //Sends command byte to OLED

    MAP_SPIDataGet(GSPI_BASE, &flush); //Reads back a dummy byte to complete transmission (I used claude to figure out this was needed)

    GPIOPinWrite(GPIOA1_BASE, 0x80, 0xff); // Sets OLEDCS high, deselect OLED

    MAP_SPICSDisable(GSPI_BASE); //Disables CS line for unselecting OLED
}


void Adafruit_Init(void) {

  volatile unsigned long delay;

  // set the initial values of RESET and OC
  GPIOPinWrite(GPIOA2_BASE, 0x2, 0);    // RESET = RESET_LOW
  GPIOPinWrite(GPIOA1_BASE, 0x80, 0); // OC = LOW

  for(delay=0; delay<100; delay=delay+1);// delay minimum 100 ns

  GPIOPinWrite(GPIOA2_BASE, 0x2, 0xff); // RESET = RESET_HIGH
  GPIOPinWrite(GPIOA1_BASE, 0x80, 0xff); // OC = HIGH

    // Initialization Sequence
  writeCommand(SSD1351_CMD_COMMANDLOCK);  // set command lock
  writeData(0x12);
  writeCommand(SSD1351_CMD_COMMANDLOCK);  // set command lock
  writeData(0xB1);

  writeCommand(SSD1351_CMD_DISPLAYOFF);         // 0xAE

  writeCommand(SSD1351_CMD_CLOCKDIV);       // 0xB3
  writeCommand(0xF1);                       // 7:4 = Oscillator Frequency, 3:0 = CLK Div Ratio (A[3:0]+1 = 1..16)

  writeCommand(SSD1351_CMD_MUXRATIO);
  writeData(127);

  writeCommand(SSD1351_CMD_SETREMAP);
  writeData(0x74);

  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(0x00);
  writeData(0x7F);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(0x00);
  writeData(0x7F);

  writeCommand(SSD1351_CMD_STARTLINE);      // 0xA1
  if (SSD1351HEIGHT == 96) {
    writeData(96);
  } else {
    writeData(0);
  }


  writeCommand(SSD1351_CMD_DISPLAYOFFSET);  // 0xA2
  writeData(0x0);

  writeCommand(SSD1351_CMD_SETGPIO);
  writeData(0x00);

  writeCommand(SSD1351_CMD_FUNCTIONSELECT);
  writeData(0x01); // internal (diode drop)
  //writeData(0x01); // external bias

//    writeCommand(SSSD1351_CMD_SETPHASELENGTH);
//    writeData(0x32);

  writeCommand(SSD1351_CMD_PRECHARGE);          // 0xB1
  writeCommand(0x32);

  writeCommand(SSD1351_CMD_VCOMH);              // 0xBE
  writeCommand(0x05);

  writeCommand(SSD1351_CMD_NORMALDISPLAY);      // 0xA6

  writeCommand(SSD1351_CMD_CONTRASTABC);
  writeData(0xC8);
  writeData(0x80);
  writeData(0xC8);

  writeCommand(SSD1351_CMD_CONTRASTMASTER);
  writeData(0x0F);

  writeCommand(SSD1351_CMD_SETVSL );
  writeData(0xA0);
  writeData(0xB5);
  writeData(0x55);

  writeCommand(SSD1351_CMD_PRECHARGE2);
  writeData(0x01);

  writeCommand(SSD1351_CMD_DISPLAYON);      //--turn on oled panel
}

/***********************************/

void goTo(int x, int y) {
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;

  // set x and y coordinate
  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(SSD1351WIDTH-1);

  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(SSD1351HEIGHT-1);

  writeCommand(SSD1351_CMD_WRITERAM);
}

unsigned int Color565(unsigned char r, unsigned char g, unsigned char b) {
  unsigned int c;
  c = r >> 3;
  c <<= 6;
  c |= g >> 2;
  c <<= 5;
  c |= b >> 3;

  return c;
}

void fillScreen(unsigned int fillcolor) {
  fillRect(0, 0, SSD1351WIDTH, SSD1351HEIGHT, fillcolor);
}

/**************************************************************************/
/*!
    @brief  Draws a filled rectangle using HW acceleration
*/
/**************************************************************************/
void fillRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int fillcolor)
{
  unsigned int i;

  // Bounds check
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
    return;

  // Y bounds check
  if (y+h > SSD1351HEIGHT)
  {
    h = SSD1351HEIGHT - y - 1;
  }

  // X bounds check
  if (x+w > SSD1351WIDTH)
  {
    w = SSD1351WIDTH - x - 1;
  }

  // set location
  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x+w-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y+h-1);
  // fill!
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < w*h; i++) {
    writeData(fillcolor >> 8);
    writeData(fillcolor);
  }
}

void drawFastVLine(int x, int y, int h, unsigned int color) {

  unsigned int i;
  // Bounds check
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
    return;

  // X bounds check
  if (y+h > SSD1351HEIGHT)
  {
    h = SSD1351HEIGHT - y - 1;
  }

  if (h < 0) return;

  // set location
  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y+h-1);
  // fill!
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < h; i++) {
    writeData(color >> 8);
    writeData(color);
  }
}



void drawFastHLine(int x, int y, int w, unsigned int color) {

  unsigned int i;
  // Bounds check
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT))
    return;

  // X bounds check
  if (x+w > SSD1351WIDTH)
  {
    w = SSD1351WIDTH - x - 1;
  }

  if (w < 0) return;

  // set location
  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x+w-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y);
  // fill!
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < w; i++) {
    writeData(color >> 8);
    writeData(color);
  }
}


void fastFillScreen(unsigned int fillcolor) {
  // Calculate total number of pixels
  unsigned int totalPixels = SSD1351WIDTH * SSD1351HEIGHT;
  unsigned char colorHigh = fillcolor >> 8;
  unsigned char colorLow = fillcolor & 0xFF;

  // Set the entire display as our active area
  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(0);
  writeData(SSD1351WIDTH-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(0);
  writeData(SSD1351HEIGHT-1);

  // Start the RAM write operation
  writeCommand(SSD1351_CMD_WRITERAM);

  // Keep DC high for data and hold CS low for the entire transfer
  GPIOPinWrite(GPIOA3_BASE, 0x10, 0xff); // DC high for data
  MAP_SPICSEnable(GSPI_BASE);            // Enable CS
  GPIOPinWrite(GPIOA1_BASE, 0x80, 0x00); // OLEDCS low

  // Send all pixels in a single CS-active session
  unsigned int i;
  for (i = 0; i < totalPixels; i++) {
    // Send high byte
    MAP_SPIDataPut(GSPI_BASE, colorHigh);
    MAP_SPIDataGet(GSPI_BASE, &flush);

    // Send low byte
    MAP_SPIDataPut(GSPI_BASE, colorLow);
    MAP_SPIDataGet(GSPI_BASE, &flush);
  }

  // End the transaction
  GPIOPinWrite(GPIOA1_BASE, 0x80, 0xff); // OLEDCS high
  MAP_SPICSDisable(GSPI_BASE);           // Disable CS
}

void fastDrawBitmap(int x, int y, const uint8_t *bitmap, int width, int height, uint16_t color, uint16_t bg_color, int pixelSize) {
    int byteWidth = (width + 7) / 8; // Bytes per row
    uint16_t colorHigh = color >> 8;
    uint16_t colorLow = color & 0xFF;
    uint16_t bgColorHigh = bg_color >> 8;
    uint16_t bgColorLow = bg_color & 0xFF;

    // Calculate the scaled width and height
    int scaledWidth = width * pixelSize;
    int scaledHeight = height * pixelSize;

    // Set the drawing window to the scaled size
    writeCommand(SSD1351_CMD_SETCOLUMN);
    writeData(x);
    writeData(x + scaledWidth - 1);
    writeCommand(SSD1351_CMD_SETROW);
    writeData(y);
    writeData(y + scaledHeight - 1);

    // Start RAM write
    writeCommand(SSD1351_CMD_WRITERAM);

    // Keep DC high for data and hold CS low for the entire transfer
    GPIOPinWrite(GPIOA3_BASE, 0x10, 0xff); // DC high for data
    MAP_SPICSEnable(GSPI_BASE);            // Enable CS
    GPIOPinWrite(GPIOA1_BASE, 0x80, 0x00); // OLEDCS low

    int j = 0;
    int i = 0;
    int py = 0;
    int px = 0;
    // Process all pixels with scaling
    for ( j = 0; j < height; j++) {
        // Repeat each row pixelSize times
        for (py = 0; py < pixelSize; py++) {
            for (i = 0; i < width; i++) {
                int byteIndex = j * byteWidth + i / 8;
                uint8_t bitMask = 0x80 >> (i & 7); // Start with leftmost bit
                bool isForeground = bitmap[byteIndex] & bitMask;

                // Repeat each pixel horizontally pixelSize times
                for (px = 0; px < pixelSize; px++) {
                    if (isForeground) {
                        // Foreground pixel
                        MAP_SPIDataPut(GSPI_BASE, colorHigh);
                        MAP_SPIDataGet(GSPI_BASE, &flush);
                        MAP_SPIDataPut(GSPI_BASE, colorLow);
                        MAP_SPIDataGet(GSPI_BASE, &flush);
                    } else {
                        if (bg_color != 1) {
                            // Background pixel
                            MAP_SPIDataPut(GSPI_BASE, bgColorHigh);
                            MAP_SPIDataGet(GSPI_BASE, &flush);
                            MAP_SPIDataPut(GSPI_BASE, bgColorLow);
                            MAP_SPIDataGet(GSPI_BASE, &flush);
                        }
                    }
                }
            }
        }
    }

    // End the transaction
    GPIOPinWrite(GPIOA1_BASE, 0x80, 0xff); // OLEDCS high
    MAP_SPICSDisable(GSPI_BASE);           // Disable CS
}

void drawPixel(int x, int y, unsigned int color)
{
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
  if ((x < 0) || (y < 0)) return;

  goTo(x, y);

  writeData(color >> 8);
  writeData(color);
}


void  invert(char v) {
   if (v) {
     writeCommand(SSD1351_CMD_INVERTDISPLAY);
   } else {
        writeCommand(SSD1351_CMD_NORMALDISPLAY);
   }
 }


