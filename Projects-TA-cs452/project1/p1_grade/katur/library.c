/**
 * @author Carter Boyd
 *
 * CSc_452, Spring 22
 *
 * This is a program designed to allow the user to manipulate teh graphics of
 * their screen using the frame buffer. the program will open a file to the
 * framebuffer, then proceeds to gather the size of the screen and mmap that
 * screen size allowing the program to have access to the screen. the program
 * will then disable key echoing and Icanon to be reactivated when the end
 * graphics is called.
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/termios.h>
#include "iso_font.h"
#include <stdio.h>
#include <stdlib.h>

#define ESCAPE_CODE "\033[2J"
#define FRAME_BUFFER "/dev/fb0"
#define STDIN 0
#define STDOUT 1
typedef unsigned short color_t;

void clear_screen();
int fD;
struct termios original;
char *map;

/**
 * Initializes the graphics library
 */
void init_graphics() {
	size_t len;
	struct fb_var_screeninfo screenRes;
	struct fb_fix_screeninfo bitDep;
	struct termios term;
	clear_screen();
	//step 1
	fD = open(FRAME_BUFFER, O_RDWR);
	//step 2 & 3
	ioctl(fD, FBIOGET_VSCREENINFO, &screenRes);
	ioctl(fD, FBIOGET_FSCREENINFO, &bitDep);
	len = screenRes.yres_virtual * bitDep.line_length;
	map = mmap(NULL, len, PROT_WRITE, MAP_SHARED, fD, 0);
	//step 4
	ioctl(STDIN, TCGETS, &original);
	term = original;
	term.c_lflag &= ~(ECHO | ICANON);
	ioctl(STDIN, TCSETS, &term);
}

/**
 * Undo whatever is needed before the program exits
 */
void exit_graphics() {
	ioctl(STDIN, TCSETS, &original);
	clear_screen();
}

/**
 * Clears the screen
 */
void clear_screen() {
	write(STDOUT, ESCAPE_CODE, 4);
}

/**
 * reads any key pressed input
 * @return the character that was inputted
 */
char getkey() {
	fd_set rFds;
	char buf[2];
	FD_ZERO(&rFds);
	FD_SET(STDIN, &rFds);
	select(1, &rFds, NULL, NULL, 0);
	read(0, buf, 1);
	return buf[0];
}

/**
 * makes the program sleep for a set period of time
 * @param ms the set period of time the program will sleep
 */
void sleep_ms(long ms) {
	nanosleep((const struct timespec *) (ms * 1000000), NULL);
}

/**
 * sets the pixel location as well as the color of where the pixel will be
 * @param x the x coordinate
 * @param y the y coordinate
 * @param color the color of the pixel
 */
void draw_pixel(int x, int y, color_t color) {
	int range;
	range = x * 1280 + y;
	if (0 <= range && range < 614400)
		map[x * 1280 + y] = color;
}

/**
 * makes a rectangle using the draw_pixel function
* @param x1 the first pixel point of the rectangle for the x position
 * @param y1 the first pixel point of the rectangle for the y position
 * @param width how wide the rectangle will be
 * @param height how tall the rectangle will be
 * @param c the color of the rectangle
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int endWidth, endHeight, i, j;
	endWidth = x1 + width * 2;
	endHeight = y1 + height;
	for(i = y1;i < endHeight && endHeight < 480 && i >= 0;++i)
		for(j = x1;j < endWidth && endWidth < 1280 && j >= 0;++j)
			draw_pixel(i, j, c);
}// remember to change this so it only shows the corners of the square

/**
 * draws a string with a specific color
 * @param x the x location of the string
 * @param y the y location of the string
 * @param text the text of the string
 * @param c the color of the string
 */
void draw_text(int x, int y, char *text, color_t c) {
	int i, j, mask, masked_n, theBit, totalI, totalJ;
	unsigned char *isoLetter;
	char *ptr;
	totalI = 0;
	totalJ = 0;
	for (ptr = text; *ptr; ++ptr) {
		isoLetter = &iso_font[(int) *ptr * 16];
		isoLetter[16] = '\0';
		for (i = 0; i < 16; ++i) {
			for (j = 7; j >= 0; --j) {
				mask = 1 << j;
				masked_n = isoLetter[i] & mask;
				theBit = masked_n >> j;
				if (theBit == 1) {
					draw_pixel(y + i, x + j + totalJ, c);
				}
			}
		}
		totalJ += 10;
	}
}
