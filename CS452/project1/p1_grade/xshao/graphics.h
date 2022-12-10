/*
 * File: graphics.h
 * Author: Xinyi Shao
 */

#ifndef PROJECTS_GRAPGICS_H
#define PROJECTS_GRAPGICS_H

typedef unsigned short color_t; // unsigned 16-bit value

// The upper 5 bits to storing red intensity (0-31),
// the middle 6 bits to store green intensity (0-63),
// the low order 5 bits to store blue intensity (0-31).
#define RGB(r, g, b) ((color_t) ((r & 0x1F) << 11) | ((g & 0x1F) << 5) | ((b & 0x1F)))

void init_graphics();

void exit_graphics();

void clear_screen();

char getkey();

void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);

void draw_rect(int x1, int y1, int width, int height, color_t c);

void draw_char(int x, int y, const char letter, color_t c);

void draw_text(int x, int y, const char *text, color_t c);

#endif //PROJECTS_GRAPGICS_H
