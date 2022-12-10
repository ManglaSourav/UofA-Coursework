#ifndef GRAPHICS_H
#define GRAPHICS_H

/*
	author: Oliver Lester
	description: This file acts as a header file for the library.c
	and driver.c files. It prototypes all the functions in both
	files
*/

typedef unsigned short color_t;

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void erase(int x1, int y1, int width, int height);
void draw_letter(int x, int y, char ch, color_t c);

void name();
void game(char* name, int s);

#endif
