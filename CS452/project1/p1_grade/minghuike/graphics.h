/*
    Assigment:  CSC452 Project 1
    Author:     Minghui Ke
    Purpose:    Head file used for library.c.
*/
#define RGB(red, green, blue) ((red << 11) | (green << 5) | blue)
typedef unsigned short color_t;

void init_graphics();

void clear_screen();

void exit_graphics();

char getkey();

void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);

void draw_rect(int x1, int y1, int width, int height, color_t c);

void draw_char(int x, int y, char ch, color_t c);

void draw_text(int x, int y, const char *text, color_t c);