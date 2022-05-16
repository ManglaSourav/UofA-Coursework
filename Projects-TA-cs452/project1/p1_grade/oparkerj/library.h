/*
 * Author: Parker Jones
 *
 * Header file for the library. Declares the functions used to initialize the
 * graphics and draw. Also provides some macros for creating colors from RGB
 * values.
 */

#ifndef PROJECT1_LIBRARY_H
#define PROJECT1_LIBRARY_H

// 16-bit color type
// R: upper 5 bits
// G: middle 6 bits
// B: lower 5 bits
typedef unsigned short color_t;

// Construct a color from RGB values
#define COLOR(r, g, b) ((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F))
// Create a 16-bit color from values in the range 0-255.
#define COLOR255(r, g, b) COLOR((r) * 31 / 255, (g) * 63 / 255, (b) * 31 / 255)

// Get the R component of a color
#define COLOR_R(color) ((color) >> 11)

// Get the G component of a color
#define COLOR_G(color) (((color) >> 5) & 0x3F)

// Get the B component of a color
#define COLOR_B(color) ((color) & 0x1F)

#define COLOR_RMAX 31
#define COLOR_GMAX 63
#define COLOR_BMAX 31

void init_graphics();

void exit_graphics();

void clear_screen();

char getkey();

void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);

void draw_rect(int x1, int y1, int width, int height, color_t c);

void draw_text(int x, int y, const char *text, color_t c);

#endif //PROJECT1_LIBRARY_H
