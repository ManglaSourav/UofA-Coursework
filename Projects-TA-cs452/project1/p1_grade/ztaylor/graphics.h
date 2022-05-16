 /* File: graphics.h
 *
 * Author: Zachary Taylor
 * NetID: ztaylor
 * Class: CSC 452
 * Assignment: HW1
 *
 */
#ifndef graphics_H
#define graphics_H

typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

#endif