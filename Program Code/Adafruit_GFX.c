/*
This is the core graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.).  It needs to be
paired with a hardware-specific library for each display device we carry
(to handle the lower-level functions).

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!
 
Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"
#include "glcdfont.h"
#include <stdint.h>
#include <stdbool.h>
//#define pgm_read_byte(addr) (*(const unsigned char *)(addr))

int cursor_x=0;
int cursor_y=0;
unsigned char textsize=1;
unsigned int textcolor = 0x0000;
unsigned int textbgcolor = 0xFFFF;
unsigned int WHITE = 0xFFFF;
unsigned int BLACK = 0x0000;
char wrap = 1;


/*
Adafruit_GFX(int w, int h):
  WIDTH(w), HEIGHT(h)
{
  _width    = WIDTH;
  _height   = HEIGHT;
  rotation  = 0;
  cursor_y  = cursor_x    = 0;
  textsize  = 1;
  textcolor = textbgcolor = 0xFFFF;
  wrap      = true;
}
*/

// Draw a circle outline
void drawCircle(int x0, int y0, int r, unsigned int color) {
  int f = 1 - r;
  int ddF_x = 1;
  int ddF_y = -2 * r;
  int x = 0;
  int y = r;

  drawPixel(x0  , y0+r, color);
  drawPixel(x0  , y0-r, color);
  drawPixel(x0+r, y0  , color);
  drawPixel(x0-r, y0  , color);

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;
  
    drawPixel(x0 + x, y0 + y, color);
    drawPixel(x0 - x, y0 + y, color);
    drawPixel(x0 + x, y0 - y, color);
    drawPixel(x0 - x, y0 - y, color);
    drawPixel(x0 + y, y0 + x, color);
    drawPixel(x0 - y, y0 + x, color);
    drawPixel(x0 + y, y0 - x, color);
    drawPixel(x0 - y, y0 - x, color);
  }
}

void drawCircleHelper( int x0, int y0,
               int r, unsigned char cornername, unsigned int color) {
  int f     = 1 - r;
  int ddF_x = 1;
  int ddF_y = -2 * r;
  int x     = 0;
  int y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;
    if (cornername & 0x4) {
      drawPixel(x0 + x, y0 + y, color);
      drawPixel(x0 + y, y0 + x, color);
    } 
    if (cornername & 0x2) {
      drawPixel(x0 + x, y0 - y, color);
      drawPixel(x0 + y, y0 - x, color);
    }
    if (cornername & 0x8) {
      drawPixel(x0 - y, y0 + x, color);
      drawPixel(x0 - x, y0 + y, color);
    }
    if (cornername & 0x1) {
      drawPixel(x0 - y, y0 - x, color);
      drawPixel(x0 - x, y0 - y, color);
    }
  }
}

void fillCircle(int x0, int y0, int r,
                  unsigned int color) {
  drawFastVLine(x0, y0-r, 2*r+1, color);
  fillCircleHelper(x0, y0, r, 3, 0, color);
}

// Used to do circles and roundrects
void fillCircleHelper(int x0, int y0, int r,
    unsigned char cornername, int delta, unsigned int color) {

  int f     = 1 - r;
  int ddF_x = 1;
  int ddF_y = -2 * r;
  int x     = 0;
  int y     = r;

  while (x<y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f     += ddF_y;
    }
    x++;
    ddF_x += 2;
    f     += ddF_x;

    if (cornername & 0x1) {
      drawFastVLine(x0+x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0+y, y0-x, 2*x+1+delta, color);
    }
    if (cornername & 0x2) {
      drawFastVLine(x0-x, y0-y, 2*y+1+delta, color);
      drawFastVLine(x0-y, y0-x, 2*x+1+delta, color);
    }
  }
}

// Bresenham's algorithm - thx wikpedia
void drawLine(int x0, int y0, int x1, int y1, unsigned int color) {
  int steep;
  int dx, dy;
    int err;
    int ystep;

    steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }

  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  dx = x1 - x0;
  dy = abs(y1 - y0);

  err = dx / 2;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0<=x1; x0++) {
    if (steep) {
      drawPixel(y0, x0, color);
    } else {
      drawPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

// Draw a rectangle
void drawRect(int x, int y,
                int w, int h,
                unsigned int color) {
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y+h-1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x+w-1, y, h, color);
}
/*
void drawFastVLine(int x, int y,
                 int h, unsigned int color) {

  // Update in subclasses if desired!
  drawLine(x, y, x, y+h-1, color);
}

void drawFastHLine(int x, int y,
                 int w, unsigned int color) {
  // Update in subclasses if desired!
  drawLine(x, y, x+w-1, y, color);
}

void fillRect(int x, int y, int w, int h,
                unsigned int color) {
  int i;

  // Update in subclasses if desired!
  for (i=x; i<x+w; i++) {
    drawFastVLine(i, y, h, color);
  }
}

void fillScreen(unsigned int color) {
  fillRect(0, 0, _width, _height, color);
}
*/
// Draw a rounded rectangle
void drawRoundRect(int x, int y, int w,
  int h, int r, unsigned int color) {
  // smarter version
  drawFastHLine(x+r  , y    , w-2*r, color); // Top
  drawFastHLine(x+r  , y+h-1, w-2*r, color); // Bottom
  drawFastVLine(x    , y+r  , h-2*r, color); // Left
  drawFastVLine(x+w-1, y+r  , h-2*r, color); // Right
  // draw four corners
  drawCircleHelper(x+r    , y+r    , r, 1, color);
  drawCircleHelper(x+w-r-1, y+r    , r, 2, color);
  drawCircleHelper(x+w-r-1, y+h-r-1, r, 4, color);
  drawCircleHelper(x+r    , y+h-r-1, r, 8, color);
}

// Fill a rounded rectangle
void fillRoundRect(int x, int y, int w,
                 int h, int r, unsigned int color) {
  // smarter version
  fillRect(x+r, y, w-2*r, h, color);

  // draw four corners
  fillCircleHelper(x+w-r-1, y+r, r, 1, h-2*r-1, color);
  fillCircleHelper(x+r    , y+r, r, 2, h-2*r-1, color);
}

// Draw a triangle
void drawTriangle(int x0, int y0,
                int x1, int y1,
                int x2, int y2, unsigned int color) {
  drawLine(x0, y0, x1, y1, color);
  drawLine(x1, y1, x2, y2, color);
  drawLine(x2, y2, x0, y0, color);
}

// Fill a triangle
void fillTriangle ( int x0, int y0,
                  int x1, int y1,
                  int x2, int y2, unsigned int color) {

  int a, b, y, last;
  int
    dx01 = x1 - x0,
    dy01 = y1 - y0,
    dx02 = x2 - x0,
    dy02 = y2 - y0,
    dx12 = x2 - x1,
    dy12 = y2 - y1;
  int
    sa   = 0,
    sb   = 0;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }
  if (y1 > y2) {
    swap(y2, y1); swap(x2, x1);
  }
  if (y0 > y1) {
    swap(y0, y1); swap(x0, x1);
  }

  if(y0 == y2) { // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if(x1 < a)      a = x1;
    else if(x1 > b) b = x1;
    if(x2 < a)      a = x2;
    else if(x2 > b) b = x2;
    drawFastHLine(a, y0, b-a+1, color);
    return;
  }



  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if(y1 == y2) last = y1;   // Include y1 scanline
  else         last = y1-1; // Skip it

  for(y=y0; y<=last; y++) {
    a   = x0 + sa / dy01;
    b   = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    drawFastHLine(a, y, b-a+1, color);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y<=y2; y++) {
    a   = x1 + sa / dy12;
    b   = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if(a > b) swap(a,b);
    drawFastHLine(a, y, b-a+1, color);
  }
}

uint16_t getByte(const uint16_t *bitmap, uint16_t byteIndex) {
    // Calculate which 16-bit word contains this byte
    uint16_t wordIndex = byteIndex / 2;
    // Determine if we need the high or low byte from the word
    if(byteIndex % 2 == 0) {
        // Even byte index - get low byte (bits 0-7)
        return bitmap[wordIndex] & 0xFF;
    } else {
        // Odd byte index - get high byte (bits 8-15)
        return (bitmap[wordIndex] >> 8) & 0xFF;
    }
}

// Updated drawBitmap function
void drawBitmap(int x, int y, const uint8_t *bitmap, int width, int height, uint16_t color,
                int pixelSize, bool drawBackground, uint16_t backgroundColor) {
    int byteWidth = (width + 7) / 8; // Bytes per row

    int j = 0;
    int i = 0;
    int py = 0;
    int px = 0;
    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i++) {
            int byteIndex = j * byteWidth + i / 8;
            uint8_t bitMask = 0x80 >> (i & 7); // Start with leftmost bit (0x80 = 10000000)

            if (bitmap[byteIndex] & bitMask) {
                // Draw a pixelSize x pixelSize square of pixels with foreground color
                for (py = 0; py < pixelSize; py++) {
                    for (px = 0; px < pixelSize; px++) {
                        drawPixel(x + i*pixelSize + px, y + j*pixelSize + py, color);
                    }
                }
            } else if (drawBackground) {
                // Draw a pixelSize x pixelSize square of pixels with background color
                for (py = 0; py < pixelSize; py++) {
                    for (px = 0; px < pixelSize; px++) {
                        drawPixel(x + i*pixelSize + px, y + j*pixelSize + py, backgroundColor);
                    }
                }
            }
        }
    }
}



// Test pattern - a simple 32x32 monochrome bitmap
// Each byte represents 8 horizontal pixels (1=on, 0=off)


// Draw a 1-bit color bitmap at the specified x, y position from the
// provided bitmap buffer (must be PROGMEM memory) using color as the
// foreground color and bg as the background color.
/*
void drawBitmap(int x, int y,
            const unsigned char *bitmap, int w, int h,
            unsigned int color, unsigned int bg) {

  int i, j, byteWidth = (w + 7) / 8;
  
  for(j=0; j<h; j++) {
    for(i=0; i<w; i++ ) {
      if(pgm_read_byte(bitmap + j * byteWidth + i / 8) & (128 >> (i & 7))) {
        drawPixel(x+i, y+j, color);
      }
      else {
        drawPixel(x+i, y+j, bg);
      }
    }
  }
}

//Draw XBitMap Files (*.xbm), exported from GIMP,
//Usage: Export from GIMP to *.xbm, rename *.xbm to *.c and open in editor.
//C Array can be directly used with this function
void drawXBitmap(int x, int y,
                              const unsigned char *bitmap, int w, int h,
                              unsigned int color) {
  
  int i, j, byteWidth = (w + 7) / 8;
  
  for(j=0; j<h; j++) {
    for(i=0; i<w; i++ ) {
      if(pgm_read_byte(bitmap + j * byteWidth + i / 8) & (1 << (i % 8))) {
        drawPixel(x+i, y+j, color);
      }
    }
  }
}

#if ARDUINO >= 100
size_t write(unsigned char c) {
#else
void write(unsigned char c) {
#endif
  if (c == '\n') {
    cursor_y += textsize*8;
    cursor_x  = 0;
  } else if (c == '\r') {
    // skip em
  } else {
    drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize);
    cursor_x += textsize*6;
    if (wrap && (cursor_x > (_width - textsize*6))) {
      cursor_y += textsize*8;
      cursor_x = 0;
    }
  }
#if ARDUINO >= 100
  return 1;
#endif
}
*/
// Draw a character
void drawChar(int x, int y, unsigned char c,
                unsigned int color, unsigned int bg, unsigned char size) {

  unsigned char line;
  char i;
  char j;

  if((x >= WIDTH)            || // Clip right
     (y >= HEIGHT)           || // Clip bottom
     ((x + 6 * size - 1) < 0) || // Clip left
     ((y + 8 * size - 1) < 0))   // Clip top
    return;

  for (i=0; i<6; i++ ) {
    if (i == 5) 
      line = 0x0;
    else 
      line = font[(c*5)+i];
    for (j = 0; j<8; j++) {
      if (line & 0x1) {
        if (size == 1) // default size
          drawPixel(x+i, y+j, color);
        else {  // big size
          fillRect(x+(i*size), y+(j*size), size, size, color);
        } 
      } else if (bg != color) {
        if (size == 1) // default size
          drawPixel(x+i, y+j, bg);
        else {  // big size
          fillRect(x+i*size, y+j*size, size, size, bg);
        }
      }
      line >>= 1;
    }
  }
}

void OutstrBlack (char * str) {
    char * ptr;

    ptr = str;
    while (*ptr) {
        drawChar(cursor_x, cursor_y, *ptr++, BLACK, WHITE, textsize);
        cursor_x += 6*textsize;
    }

}

void Outstr (char * str, int COLOR, int BACKGROUND_COLOR, int x1, int y1, int x2, int y2) {
    char * ptr;
    cursor_x = x1;
    cursor_y = y1;
    ptr = str;
    while (*ptr) {
        drawChar(cursor_x, cursor_y, *ptr++, COLOR, BACKGROUND_COLOR, textsize);
        cursor_x += 6*textsize;
        if((cursor_x + 6*textsize)>= x2){
            cursor_y += 10*textsize;
            cursor_x = x1;
        }

    }

}

void Outstrpretty (char * str, int COLOR, int BACKGROUND_COLOR, int x1, int y1, int x2, int y2) {
    char * ptr;
    char * temp_ptr;
    int word_width;

    cursor_x = x1;
    cursor_y = y1;
    ptr = str;

    while (*ptr) {
        /* If it's a space, handle it normally */
        if (*ptr == ' ') {
            if ((cursor_x + 6*textsize) >= x2) {
                cursor_y += 10*textsize;
                cursor_x = x1;
            } else {
                drawChar(cursor_x, cursor_y, *ptr, COLOR, BACKGROUND_COLOR, textsize);
                cursor_x += 6*textsize;
            }
            ptr++;
        }
        else {
            /* Start of a word - calculate word length */
            temp_ptr = ptr;
            word_width = 0;

            /* Calculate width of current word */
            while (*temp_ptr && *temp_ptr != ' ') {
                word_width += 6*textsize;
                temp_ptr++;
            }

            /* If word doesn't fit on current line and we're not at start, wrap */
            if ((cursor_x + word_width >= x2) && (cursor_x > x1)) {
                cursor_y += 10*textsize;
                cursor_x = x1;
            }

            /* Draw characters of the word */
            while (ptr < temp_ptr) {
                /* Force break if even single characters don't fit (very long words) */
                if ((cursor_x + 6*textsize) >= x2) {
                    cursor_y += 10*textsize;
                    cursor_x = x1;
                }
                drawChar(cursor_x, cursor_y, *ptr, COLOR, BACKGROUND_COLOR, textsize);
                cursor_x += 6*textsize;
                ptr++;
            }
        }
    }
}

void OutstrWhite(char * str) {
    char * ptr;

    ptr = str;
    while (*ptr) {
        drawChar(cursor_x, cursor_y, *ptr++, WHITE, WHITE, textsize);
        cursor_x += 6*textsize;
    }

}
// Add these functions to your existing code


void setCursor(int x, int y) {
  cursor_x = x;
  cursor_y = y;
}

void setTextSize(unsigned char s) {
  textsize = (s > 0) ? s : 1;
}
/*
void setTextColor(unsigned int c) {
  // For 'transparent' background, we'll set the bg 
  // to the same as fg instead of using a flag
  textcolor = textbgcolor = c;
}
*/

void setTextColor(unsigned int c, unsigned int b) {
  textcolor   = c;
  textbgcolor = b; 
}


void setTextWrap(char w) {
  wrap = w;
}
/*
unsigned char getRotation(void) {
  return rotation;
}


void setRotation(unsigned char x) {
  rotation = (x & 3);
  switch(rotation) {
   case 0:
   case 2:
    _width  = WIDTH;
    _height = HEIGHT;
    break;
   case 1:
   case 3:
    _width  = HEIGHT;
    _height = WIDTH;
    break;
  }
}

*/
// Return the size of the display (per current rotation)
int width(void) {
//  return _width;
    return WIDTH;
}
 
int height(void) {
//  return _height;
    return HEIGHT;
}


