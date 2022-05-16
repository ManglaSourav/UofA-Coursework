/**
 * @file library.c
 * @author Luke Broadfoot (lucasxavier@email.arizona.edu)
 * @brief Project 1: Graphics Library
 * This file contains a rudimentary graphics library capiable of drawing
 * a pixel to the framebuffer, a solid rectangle, and text
 * (all with full 16-bit color).
 * @version 1.0
 * @date 2022-02-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "iso_font.h"
#include "graphics.h"
#include <time.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <termios.h>
#include <sys/uio.h>
#include <unistd.h>

// establishes private variables for library.c
struct fb_var_screeninfo fbVarInfo;
struct fb_fix_screeninfo fixInfo;
void *fbmem;
struct termios terminal;
int screen_size;
int fd;

/**
 * @brief Opens the framebuffer as read and write, gets screen resolution
 * from ioctl call, using mmap gets the memory address of the whole screen.
 * Using ioctl to turn off the ICANON and ECHO bits then updates the termios
 * 
 */
void init_graphics() {
	// gets the file descriptor for the framebuffer
	fd = open("/dev/fb0", O_RDWR);
	// populates the fb_var/fix_screeninfo structs based on the framebuffer
	ioctl(fd, FBIOGET_VSCREENINFO, &fbVarInfo);
	ioctl(fd, FBIOGET_FSCREENINFO, &fixInfo);
	// gets the total screen size in bytes
	screen_size = fbVarInfo.yres_virtual*fixInfo.line_length;
	// gets the memory address of the framebuffer
	fbmem = mmap(NULL, screen_size, PROT_WRITE, MAP_SHARED, fd, 0);
	// populates the termios struct
	ioctl(0, TCGETS, &terminal);
	// turns off the ICANON and ECHO bits
	terminal.c_lflag &= ~(ICANON|ECHO);
	// terminal.c_lflag &= ~ECHO;
	// calls TCSETS to update the termios
	ioctl(0, TCSETS, &terminal);
}

/**
 * @brief turns ICANON and ECHO back on, closes the file descriptor and calls
 * munmap.
 * 
 */
void exit_graphics() {
	// turns on the ICANON and ECHO bit
	terminal.c_lflag |= (ICANON|ECHO);
	// terminal.c_lflag |= ECHO;
	// calls TCSETS
	ioctl(0, TCSETS, &terminal);
	close(fd);
	munmap(fbmem, screen_size);
}

/**
 * @brief writes the ansi escape code to stdout
 * 
 */
void clear_screen() {
	write(1, "\033[2J", 4);
}

/**
 * @brief Uses select to determine if there is anything in the stdin buffer
 * if something is there, returns the character siting there
 * 
 * @return char the current character in the stdin buffer
 */
char getkey() {
	char res;
	// gets a fd_set struct and sets it to listen to stdin
	fd_set rd;
	FD_ZERO(&rd);
	FD_SET(0, &rd);
	// sets a timeval struct to block for only 500 microseconds
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500;
	// if select returns a positive value that means something is there
	if (select(1, &rd, NULL, NULL, &tv)) {
		read(0, &res, 1);
	}
	return res;
}

/**
 * @brief This function tells the thread to sleep for ms milliseconds.
 * 
 * @param ms how long to sleep in milliseconds
 */
void sleep_ms(long ms) {
	// makes a timespec struct and sets the seconds and nanoseconds
	struct timespec req;
	req.tv_sec = 0;
	req.tv_nsec = ms * 1000000;
	nanosleep(&req, NULL);
}

/**
 * @brief Draws a single pixel to the framebuffer
 * 
 * @param x the x coordinate
 * @param y the y coordinate
 * @param color a 16-bit color
 */
void draw_pixel(int x, int y, color_t color) {
	// uses pointer manipulation to get to the correct location
	void *fb = fbmem + (2 * x) + (fixInfo.line_length * y);
	// type coercion to unsigned short
	unsigned short *cur = fb;
	// assigns the color to the current pointer
	*cur = color;
}

/**
 * @brief Used to draw a solid rectangle from the top-left to bottom-right
 * 
 * @param x1 the starting x position
 * @param y1 the starting y position
 * @param width the width of the rectangle
 * @param height the height of the rectangle
 * @param c a 16-bit color
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int i, j;
	for (i = x1; i < x1+width; i++) {
		for (j = y1; j < y1+height; j++) {
			draw_pixel(i, j, c);
		}
	}
}

/**
 * @brief Used to draw text using the provided iso_font file
 * 
 * @param x the starting x position
 * @param y the starting y position
 * @param text a const char * of the text to print
 * @param c a 16-bit color
 */
void draw_text(int x, int y, const char *text, color_t c) {
	// the first character in the const char *
	char cur = text[0];
	int i = 0, j, k;
	// used to handle bit manipulation
	unsigned char character, mask;
	// loops until we hit the end of the string
	while (cur != '\0') {
		// each character is 16 rows of 1 bytes
		for (j = 0; j < 16; j++) {
			// indexed the font array at the specific character and row
			character = iso_font[(cur*16)+j];
			// used to mask off the specific bits at each location
			mask = 0x80;
			for (k = 0; k < 8; k++) {
				// if the masked bit is 1 we draw a pixel
				if ((character & mask) >> (7-k) == 1) {
					draw_pixel(x-k +(8*i), y+j, c);
				}
				// moves the mask over 1
				mask = mask >> 1;
			}
		}
		i++;
		cur = text[i];
	}
}
