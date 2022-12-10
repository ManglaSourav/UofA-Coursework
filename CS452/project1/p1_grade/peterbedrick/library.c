/*
 * Author: Peter Bedrick
 * Class: CSC 452
 * Program: Project 1
 * File: library.c
 * Purpose: Handles key input and drawing pixels
 * to make up rectangles and text with inputted color
 */

#include "graphics.h"
#include <sys/mman.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <termios.h>
#include <sys/select.h>
#include <stdio.h>
#include <fcntl.h>
#include "iso_font.h"

typedef unsigned short color_t;

struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;
struct termios term;
int file;
char *mapdata;

// opens graphical file, and initializes drawing mode
void init_graphics() {
	int file = open("/dev/fb0", O_RDWR);
	int i1 = ioctl(file, FBIOGET_VSCREENINFO, &var);
	int i2 = ioctl(file, FBIOGET_FSCREENINFO, &fix);
	mapdata = mmap(0, var.yres_virtual * fix.line_length, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
	ioctl(0, TCGETS, &term);
	term.c_lflag &= ~ECHO;
	term.c_lflag &= ~ICANON;
	ioctl(0, TCSETS, &term);
	clear_screen();
}

// closes graphical file, and resets flags
void exit_graphics() {
	ioctl(0, TCGETS, &term);
	term.c_lflag |= ECHO;
	term.c_lflag |= ICANON;
	ioctl(0, TCSETS, &term);	
	munmap(NULL, 0);
	close(file);
}

// clears the screen of all pixels including previously written text
void clear_screen() {
	write(1,"\033[2J",4);	
}

// gets key press info and returns it
char getkey() {
	fd_set curr;
	FD_ZERO(&curr);
	FD_SET(0, &curr);
	struct timeval t;
	t.tv_sec = 0;
	t.tv_usec = 0;
	int val = select(1, &curr, 0, 0, &t);
	int r;
	if(val == 0) {
		return r;
	}
	unsigned char c;
	if((r = read(0, &c, sizeof(c))) < 0) {
		return r;
	} else {
		return c;
	}
}

// pauses program for inputted # of milliseconds
void sleep_ms(long ms) {
	struct timespec sleep;
	sleep.tv_sec = 0;
	sleep.tv_nsec = ms * 1000000;
	nanosleep(&sleep, NULL);
}

// counts length of a string
int length(const char *string) {
	if(*string == '\0') {
		return 0;	
	}	
	return (length(++string) + 1);
}

// draws a singular pixel at specified coordinate with specified color
void draw_pixel(int x, int y, color_t color) {
	int pos = (y * var.xres + x) * 2;
	unsigned mask;
	mask = (1 << 5) - 1;
	int b = color & mask;
	mask = ((1 << 6) - 1) << 5;
	int g = color & mask;
	mask = ((1 << 5) - 1) << 11;
	int r = color & mask;
	if(y < var.yres) {
		mapdata[pos] = b;
		mapdata[pos + 1] = g;
		mapdata[pos + 2] = r;
	}
}

// uses draw_pixel to draw a rectangle outline with given specifications
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int i = 0;
	for(i = 0; i < width; i++) {
		draw_pixel(x1 + i, y1, c);
		draw_pixel(x1 + i, y1 + height, c);
	}
	for(i = 0; i < height; i++) {
		draw_pixel(x1, y1 + i, c);
		draw_pixel(x1 + width, y1 + i, c);
	}
}

// uses draw_pixel to draw text with specifications
void draw_text(int x, int y, const char *text, color_t c) {
	int i = 0;
	for(i = 0; i < length(text); i++) {
		int j = 0;
		for(j = 0; j < 16; j++) {
			int r = iso_font[text[i] * 16 + j];
			int b = 0;
			for(b = 0; b < 8; b++) {
				int val = (r >> 7 - b) & 1;
				if(val != 0) {
					draw_pixel(x * (i + 1) + 7 - b, y + j, c);
				}
			}
		}
	}	
}
