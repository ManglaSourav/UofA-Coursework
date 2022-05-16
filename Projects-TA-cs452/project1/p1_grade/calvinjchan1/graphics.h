/*
Filename: graphics.h
Author: Ember Chan
Course: CSC452 Spr 2022
Description: Header file for graphics library
*/

#include <stdint.h>

#ifndef GRAPHICS_H
#define GRAPHICS_H

typedef int16_t color_t;

//Initialize graphics library
extern void init_graphics();

//Exit graphics library
extern void exit_graphics();

//Clear the screen
extern void clear_screen();

//Returns the next keypress in queue
extern char getkey();

//Sleeps for the given number of miliseconds
extern void sleep_ms(long ms);

//Draws a pixel to the screen
extern void draw_pixel(int x, int y, color_t color);

//Draws a rectangle to the screen
extern void draw_rect(int x1, int y1, int width, int height, color_t c);

//draws text to the screen
extern void draw_text(int x, int y, const char *text, color_t c);

#endif
