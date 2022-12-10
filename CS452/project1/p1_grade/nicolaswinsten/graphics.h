#ifndef GRAPHICS
#define GRAPHICS

typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char* text, color_t color);
void fill_rect(int x1, int y1, int width, int height, color_t c);

color_t rgb(unsigned int r, unsigned int g, unsigned int b);
#endif
