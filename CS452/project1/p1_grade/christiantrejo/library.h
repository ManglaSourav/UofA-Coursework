#ifndef __LIBRARY_H__INCLUDED__
#define __LIBRARY_H__INCLUDED__

typedef unsigned short color_t;

color_t getRGB(color_t red, color_t green, color_t blue);

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_char(int x, int y, char c, color_t color);
void draw_text(int x, int y, const char *text, color_t c);

#endif
