/*
 * File: library.c
 * Author: Edwin Renck
 * Purpose: This file implements a graphics library that can be used by other
 * programs.
 */

// nanosleep
#include <time.h>
// open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// mmap
#include <sys/mman.h>
// ioctl
#include <sys/ioctl.h>
// close
#include <unistd.h>
// linux headers
#include <linux/fb.h>
// terminal flags
#include <termios.h>
// select
#include <sys/select.h>

#include "iso_font.h"

#include <stdio.h>

// global variable for graphics
typedef unsigned int color_t;
char *videoOut;
int fd, yres, xres;

/*
 * clear_screen() -- makes the screen a blank screen
 */
void clear_screen() {
	char *blank = "\e[2J";
	write(0, blank, 5);
}

/*
 * init_graphics() -- initiates the graphics information by opening the graphics
 * device, and mapping it to memory.
 */
void init_graphics() {
	// open graphics driver
	fd = open("/dev/fb0", O_RDWR);

	// gets screen resolution
	struct fb_var_screeninfo varscreeninfo;
	ioctl(fd, FBIOGET_VSCREENINFO, &varscreeninfo);
	yres = varscreeninfo.yres_virtual;

	struct fb_fix_screeninfo fixscreeninfo;
	ioctl(fd, FBIOGET_FSCREENINFO, &fixscreeninfo);
	xres = fixscreeninfo.line_length;

	// maps screen to memory
	videoOut = mmap(NULL,
					(yres * xres),
					PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	// stops echoing from terminal
	struct termios term;
	ioctl(0, TCGETS, &term);
	term.c_lflag &= ~ICANON;
	term.c_lflag &= ~ECHO;
	ioctl(0, TCSETS, &term);

	clear_screen();
}

/*
 * exit_graphics() -- clears the graphics information by unmapping the graphics
 * device to memory, and closing the connection.
 */
void exit_graphics() {
	// close the file
	close(fd);

	// unmap memory
	munmap(videoOut, (yres * xres));

	// reenables echos
	struct termios term;
	ioctl(0, TCGETS, &term);
	term.c_lflag |= ICANON;
	term.c_lflag |= ECHO;
	ioctl(0, TCSETS, &term);

	clear_screen();
}

/*
 * getkey() -- reads a key from the keyboard, returs a character representing
 * the key.
 */
char getkey() {
	fd_set readfds;
	struct timeval timeout;
	FD_ZERO(&readfds);
	FD_SET(0, &readfds);

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	char buf[2];

	if(select(1, &readfds, NULL, NULL, &timeout) > 0) {
		read(0, (void *) buf, 1);
		return buf[0];
	}

	return -1;
}

/*
 * sleep_ms(ms) -- sleeps for a number of milliseconds
 */
void sleep_ms(long ms) {
	struct timespec required;

	// Calculate seconds
	long seconds = ms / 1000;
	required.tv_sec = seconds;

	// Calculate remainder milliseconds
	long nano = ms % 1000;
	required.tv_nsec = nano * 1000000;

	// Sleep system call
	nanosleep(&required, NULL);
}

/*
 * draw_pixel(x, y, color) -- draws a singular pixel with a specific color to
 * the screen.
 */
void draw_pixel(int x, int y, color_t color) {
	if (x >= xres || y >= yres) return;
	if (x < 0 | y < 0) return;
	videoOut[y * xres + x] = color;
}

/*
 * draw_rect(x1, x1, width, height, c) -- draws a rectangle with starting
 * coordinates x1, y1 with the specified width and height, and color.
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int x, y;
	for(x = x1; x < x1 + width; x++) {
		for(y = y1; y < y1 + height; y++) {
			draw_pixel(x, y, c);
		}
	}
}

/*
 * draw_char(x, y, letter, c) -- draws a single character to the screen.
 */
void draw_char(int x, int y, char letter, color_t c) {
	int px, py;
	unsigned char l;
	for(py = 0; py < ISO_CHAR_HEIGHT; py++) {
		l = iso_font[(letter * ISO_CHAR_HEIGHT) + py];
		for(px = 0; px < 8; px++) {
			int a = 1 & l;
			if (a) {
				draw_pixel(x + px, y + py, c);
			}
			l >>= 1;
		}
	}
}

/*
 * draw_text(x, y, *text, c) -- draws the text starting at location x,y. If the
 * line is too big, the text wraps around to the next position.
 */
void draw_text(int x, int y, const char *text, color_t c) {
	int count = 0;
	const char *letter;
	for(letter = text; *letter != '\0'; letter++) {
		draw_char(x + (count * 8), y, *letter, c);
		count++;
	}
}

