/*
Author: Emilio Santa Cruz
Class: 452 @ Spring 2022
Professor: Dr. Misurda
Assignment: Project 1
Description: The header file of the project to share the required 
functions to the driver.
*/


#ifndef graphics
#define graphics

typedef unsigned short color_t;

void clear_screen(void);
void init_graphics(void);
void exit_graphics(void);
char getkey(void);
void sleep_ms(long);
void draw_pixel(int, int, color_t);
void draw_rect(int, int, int, int, color_t);
void draw_text(int, int, const char*, color_t);

#endif
