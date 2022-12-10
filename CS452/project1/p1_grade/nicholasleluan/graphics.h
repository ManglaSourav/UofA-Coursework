#ifndef GRAPHICS_DOT_H
#define GRAPHICS_DOT_H

typedef unsigned short color_t;

/*
functions from library.c
*/
void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);
void draw_letter(int x, int y, int letter, color_t c);
color_t get_RGB_565(int r, int g, int b);

#endif
