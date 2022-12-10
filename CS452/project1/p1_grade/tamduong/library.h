#ifndef library_h

typedef unsigned short color_t;

#define RGB(r, g, b) ((color_t) ((r) << 11) | (g) << 5 | (b))

#define library_h

void init_graphics();

void exit_graphics();

void clear_screen();

char get_key();

void sleep_ms(long);

void draw_pixel(int, int, color_t);

void draw_rect(int, int, int, int, color_t);

void draw_text(int, int, const char *, color_t);

#endif