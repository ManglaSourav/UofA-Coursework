#ifndef DRAWING_LIBRARY_H
#define DRAWING_LIBRARY_H
/* Author: Cole Blakley
   Description: Declarations/constants used to do simple drawing to
    the screen buffer.
*/

typedef unsigned short color_t;
color_t make_color(unsigned short r, unsigned short g, unsigned short b);

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define FONT_WIDTH 8
#define FONT_HEIGHT 16

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char* text, color_t c);

void draw_char(int x, int y, char letter, color_t c);
#endif
