/**
Author: Hassan Alnamer
This file builds a graphics library that will operate on 
/dev/fb0/
It will be a library used by  driver.c

Functions built in the file:
	void init_graphics() ->open, ioctl, mmap
	void exit_graphics() -> ioctl
	void clear_screen()-> write
	char getkey()-> select, read
	void sleep_ms(long ms) -> nanosleep
	void draw_pixel(int x, int y, color_t color) 
	void draw_rect(int x1, int y1, int width, int width, 
	color_t c)
	void draw_text(int x, int y, const char* text, 
	color_t c)
	
	
	
*/
#ifndef GRAPHICS_H
#define GRAPHICS_H
int is_init;
typedef unsigned short int color_t;
#define CONVERT(R, G, B) (((R & 0x1F) << 11) | ((G & 0x3F) << 5) | (B & 0x1F))
void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char* text, color_t c);

#endif

