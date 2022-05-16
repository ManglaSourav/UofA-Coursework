/* File: graphics.h
   Author: Samantha Mathis
   Purpose: defines methods for the graphics library
*/

typedef unsigned short color_t;

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x, int y, int width, int height, color_t color);
void fill_rect(int x, int y, int width, int height, color_t color);
void draw_text(int x, int y, const char *text, color_t color);

