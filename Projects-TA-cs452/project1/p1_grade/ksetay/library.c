/*
 * File: library.c
 * Author: Kaiden Yates
 * Implements a graphics library using only syscalls
 */

#include <fcntl.h>
#include <linux/fb.h>

#include "iso_font.h"

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <termios.h>

#include <time.h>

#include <unistd.h>

typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);
void draw_char(int x, int y, char letter, color_t c);

long int size = 0;
int screen_x = 0;
int screen_y = 0;
color_t* map;

void init_graphics() {
	int fb = 0;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;

	//Opens the FrameBuffer file
	fb = open("/dev/fb0", O_RDWR);

	//Gets the screen info
	if (ioctl(fb, FBIOGET_FSCREENINFO, &finfo) == -1) {
		return;
	}
	if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		return; 
	}
	
	//Stores the screen size information
	size = vinfo.yres_virtual * finfo.line_length;
	screen_x = vinfo.xres;
	screen_y = vinfo.yres;

	//Maps the frambuffer in memory
	map = (color_t*)mmap(0, size, PROT_READ | PROT_WRITE, 
	MAP_SHARED, fb, 0);
	if ((int)map == -1) {
		return;
	}
	
	//Disables key echoing
	struct termios ter;
	if (ioctl(0, TCGETS, &ter) == -1) {
		return;
	}
	ter.c_lflag &= ~(ICANON | ECHO);
	if (ioctl(0, TCSETS, &ter) == -1) {
		return;
	}
}

// Renables key echoing
void exit_graphics() {
	struct termios ter;
	if (ioctl(0, TCGETS, &ter) == -1) {
		return;
	}
	ter.c_lflag |= (ICANON | ECHO);
	if (ioctl(0, TCSETS, &ter) == -1) {
		return;
	}
}

//Clears the screen by drawing a black rectangle
void clear_screen() {
	int x = 0;
	int y = 0;
	for (y; y < screen_y; y++) {
		for (x = 0; x < screen_x; x++) {
			draw_pixel(x, y, 0);
		}
	}
}

//Non blocking call that returns a char if key was pressed
char getkey() {
	char input;
	fd_set rfds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_SET(0, &rfds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(1, &rfds, NULL, NULL, &tv) == 1) {
		read(0, &input, 1);
		return input;
	}
	else {
		return 0;
	}
}

//Sleeps for a given number of milleseconds
void sleep_ms(long ms) {
	long nano = ms;
	time_t sec = 0;
	if (ms >= 1000) {
		nano = ms % 1000;
		sec = ms / 1000;
	}
	struct timespec time;
	time.tv_sec = sec;
	time.tv_nsec = nano * 1000000;
	if (nanosleep(&time, NULL) == -1) {
		return;
	}
}

//Worker code that draws a pixel at a given x,y coord
void draw_pixel(int x, int y, color_t color) {
	long int offset = (x + (y * 640));
	*((color_t*)(map + offset)) = color; 
}

//Draws a rectangle with a top left corner at x1,y1
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int h = y1;
	int w = x1;
	for (h; h < (y1 + height) && h < screen_y; h++) {
		for (w = x1; w < (x1 + width) && w < screen_x; w++) {
			draw_pixel(w, h, c);
		}
	}

}

//Prints a string to the display using the iso_font font
void draw_text(int x, int y, const char *text, color_t c) {
	const char* ptr;
	for (ptr = text; *ptr != '\0'; ptr++) {
		if (x + 8 > screen_x || y + 16 > screen_y) {
			return;
		}
		draw_char(x, y, *ptr, c);
		x += 8;
	}
}

//Prints a single char, used in draw text
void draw_char(int x, int y, char letter, color_t c) {
	int row = 0;
	char* ptr = iso_font;
	for (row; row < 16; row++) {
		int i = 0;
		unsigned char mask = 1;
		for (i; i < 8; i++) {
			unsigned char rowbyte = *(ptr + (letter * 16) + 
			4);
			if (mask & *(ptr + (letter * 16) + row)) {
				draw_pixel((x+i), (y + row), c);
			}
			mask = mask << 1;
		}
	}
}
