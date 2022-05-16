/*
 * graphics.h
 *
 * Author: Connie Sun
 * Course: CSC 452 Spring 2022
 * 
 * The header file associated with library.c. Declares the 
 * functions used in the graphics library and defines some
 * basic colors.
 *
 */

#ifndef GRAPHICS_H_
#define GRAPHICS_H_

#define BLACK 0
#define WHITE get_color_from_rgb(15, 31, 15)
#define BLUE get_color_from_rgb(0, 0, 15)
#define RED get_color_from_rgb(15, 0, 0)
#define GREEN get_color_from_rgb(0, 31, 0)
#define MAGENTA get_color_from_rgb(15, 0, 15)
#define YELLOW get_color_from_rgb(15, 31, 0)
#define CYAN get_color_from_rgb(0, 31, 15)

typedef unsigned short color_t;

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t c);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);
void draw_char(int x, int y, char c, color_t color);
color_t get_color_from_rgb(color_t r, color_t g, color_t b);

#endif /* GRAPHICS_H_ */
