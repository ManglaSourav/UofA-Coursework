/*
 * File: graphics.h
 * Author: Gavin Vogt
 * Purpose: Defines the 16-bit color type and provides a macro for creating it.
 * Also creates method prototypes.
 */

#ifndef _PROJECT_1_GRAPHICS_H
#define _PROJECT_1_GRAPHICS_H

// Color type is an unsigned 16-bit value
typedef unsigned short color_t;

#define FROM_RGB(r, g, b) ((r << 11) | (g << 5) | b)

// Method prototypes
void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

#endif
