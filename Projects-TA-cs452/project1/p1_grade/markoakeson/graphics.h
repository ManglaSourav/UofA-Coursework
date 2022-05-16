/**
 * Author:  Mark Oakeson
 * Class: CSc 452
 * Instructor: Misurda
 * Project:  1
 * File: graphics.h
 *
 *
 * Description: A header file to be used in the driver.c file
 */

typedef unsigned short color_t;

void clear_screen();

color_t createColor(int r, int g, int b);

void init_graphics();

void exit_graphics();

char getkey();

void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);

void draw_rect(int x1, int y1, int width, int height, color_t c);

void draw_text(int x, int y, const char *text, color_t c);