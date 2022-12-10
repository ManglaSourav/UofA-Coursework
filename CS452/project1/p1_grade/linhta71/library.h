#ifndef LIBRARY_H

typedef unsigned short color_t;

#define RGB(r, g, b) ((color_t) ((r) << 11) | (g) << 5 | (b))

#define LIBRARY_H

void init_graphics();

void exit_graphics();

void clear_screen();

char get_key();

void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);

void draw_rect(int x1, int y1, int width, int height, color_t c);

void draw_text(int x, int y, const char *text, color_t c);

void draw_char(int x, int y, char character, color_t c);

#endif