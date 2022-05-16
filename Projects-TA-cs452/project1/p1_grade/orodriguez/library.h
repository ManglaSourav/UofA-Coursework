#ifndef LIBRARY_H_
#define LIBRARY_H_
/*
Orlando Rodriguez
CSC 452 Spring 2022
Project 1 Header file
*/

typedef unsigned short color_t;

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long);
void draw_pixel(int, int, color_t);
void draw_rect(int, int, int, int, color_t);
void draw_text(int, int, const char*, color_t);
color_t encodeRGB(int, int, int);

#endif
