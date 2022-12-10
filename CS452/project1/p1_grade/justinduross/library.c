/*
 * File: library.c
 * Author: Justin Duross
 * Purpose: This file acts as a small graphics library
 * 	that can set a pixel to a particular color, draw
 *	basic shapes, and read keypresses.
*/
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <termios.h>
#include <sys/select.h>
#include <unistd.h>
#include "iso_font.h"

void *mem;
int fbFile;
int terminalFile;
struct fb_var_screeninfo vscreen;
struct fb_fix_screeninfo fscreen;
typedef unsigned short color_t;

/*
 * This function initializes the graphics library by opening the
 * framebuffer file and maps it into the system memory. It also disables
 * keypress echo and buffering keypresses.
*/
void init_graphics() {
	fbFile = open("/dev/fb0", O_RDWR);

	ioctl(fbFile, FBIOGET_VSCREENINFO, &vscreen);
	ioctl(fbFile, FBIOGET_FSCREENINFO, &fscreen);

	mem = mmap(NULL, vscreen.yres_virtual*fscreen.line_length,
	PROT_READ | PROT_WRITE, MAP_SHARED, fbFile, 0);

	int terminalFile = open("/dev/tty", O_RDWR);
	struct termios termStruct;
	ioctl(terminalFile, TCGETS, &termStruct);
	termStruct.c_lflag &= ~ICANON;
	termStruct.c_lflag &= ~ECHO;
	ioctl(terminalFile, TCSETS, &termStruct);
}

/*
 * This function closes cleans up the graphics library before the
 * program exits. It reenables keypress echoing and key buffering, along
 * with unmaping the framebuffer from memory and closing the opened
 * files.
*/
void exit_graphics() {
	struct termios termStruct;
	ioctl(terminalFile, TCGETS, &termStruct);
	termStruct.c_lflag |= (ICANON | ECHO);

	ioctl(terminalFile, TCSETS, &termStruct);

	ioctl(fbFile, FBIOGET_VSCREENINFO, &vscreen);
	ioctl(fbFile, FBIOGET_FSCREENINFO, &fscreen);
	munmap(mem, vscreen.yres_virtual*fscreen.line_length);

	close(fbFile);
	close(terminalFile);
}

/*
 * This function simply uses an ANSI escape code that is printed to the
 * terminal that clears the whole screen.
*/
void clear_screen() {
	write(terminalFile, "\033[2J", sizeof(char)*4);
}

/*
 * This function uses the non-blocking system call select to see if
 * there is a keypress waiting to be read. If there is not then it
 * returns the NULL char, but if there is a keypress then it will
 * read it and return the char that was pressed.
*/
char getkey() {
	fd_set selectRead;
	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;
	FD_ZERO(&selectRead);
	FD_SET(terminalFile, &selectRead);
	select(terminalFile + 1, &selectRead, NULL, NULL, &time);
	char key;
	if (FD_ISSET(terminalFile, &selectRead)) {
		read(terminalFile, &key, sizeof(char));
		return key;
	}

	return '\0';
}

/*
 * This function simply uses the ms parameter and calls the nanosleep()
 * system call in order to sleep for a specified number of milliseconds.
*/
void sleep_ms(long ms) {
	struct timespec time;
	if (ms >= 1000) {
		time.tv_sec = (int) ms/1000;
	}
	else {
		time.tv_sec = 0;
	}
	time.tv_nsec = (ms % 1000) * 1000000;
	nanosleep(&time, NULL);
}

/*
 * This function takes in an x location, y location, and color value for
 * the individual pixel. It uses the screeninfo structs to calculate
 * the location of the pixel in the mapped memory and sets it to the
 * color parameter.
*/
void draw_pixel(int x, int y, color_t color) {
	int pixel = (x * 2) + (y * fscreen.line_length);
	*( (color_t*) (mem + pixel) ) = color;
}

/*
 * This function draws a solid rectangle to the screen using the x1 and
 * y1 as the upper left corner and the width and the height of the
 * rectangle and the color.
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int i;
	for (i = x1; i < x1+width; i++) {
		int j;
		for (j = y1; j < y1+height; j++) {
			draw_pixel(i, j, c);
		}
	}
}

/*
 * This function draws a specific letter to the screen where the upper
 * left corner is at x,y with a specific color. Uses the iso_font.h font
 * array and uses shifting a masking to color in every pixel that is
 * flagged in the font array for that letter.
*/
void draw_letter(int x, int y, const char *letter, color_t c) {
	int ascii_value = (int) *letter;
	int font_start = ascii_value * 16;
	int i;
	for (i = 0; i < 16; i++) {
		char curr_line = iso_font[font_start + i];
		int curr_bit;
		for (curr_bit = 0; curr_bit < 8; curr_bit++) {
			if ( (curr_line >> curr_bit) & 0x1) {
				draw_pixel(x + curr_bit, y + i, c);
			}
		}
	}
}

/*
 * This function draws text to the screen starting at the x,y location
 * and with the designated color c. It iterates through each char in the
 * char array and calls draw_letter() until the NULL char is encountered
*/
void draw_text(int x, int y, const char *text, color_t c) {
	const char *letter = text;
	int letter_offset = 0;
	while (*letter != '\0') {
		draw_letter(x + letter_offset, y, letter, c);
		letter++;
		letter_offset += 8;
	}
}
