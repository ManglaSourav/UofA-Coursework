/*
* File: graphics.h
* Author: Kaden Herr
* Date Created: Jan 2022
* Last editted: Feb 5, 2022
* Purpose: Header file for library.c
*/
#ifndef GRAPHICS_H
#define GRAPHICS_H

/* Types */
typedef unsigned short color_t;

/* Prototypes */
color_t encode_color(int r, int g, int b);
void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void fill_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

#endif