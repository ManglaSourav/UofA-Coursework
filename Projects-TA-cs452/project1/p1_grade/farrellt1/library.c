/*
Author: Tristan Farrell
Class: CSC 452
Date: Feb 6, 2022
Description: library.c creates a graphics library using linux system
calls. It has functions to draw pixels, rectangles, and text. It also
has functions to sleep the program and to get a user's key press.
*/

#include <sys/mman.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/fb.h>
#include <termios.h>
#include <fcntl.h>
#include "iso_font.h"

typedef unsigned short color_t;

// GLOBAL VARIABLES
struct fb_var_screeninfo vhead;
struct fb_fix_screeninfo fhead;
struct termios thead;
int file;
char *data;

/*
 init_graphics initializes the graphics library by opening the
 framebuffer, getting the screen info, modifiying the termios, and
 mapping the fb memory to use later.
 */
void init_graphics(){
	file = open("/dev/fb0", O_RDWR);

	ioctl(file, FBIOGET_VSCREENINFO, &vhead);
	ioctl(file, FBIOGET_FSCREENINFO, &fhead);

	ioctl(0, TCGETS, &thead);
	thead.c_lflag &= ~ECHO;
	thead.c_lflag &= ~ICANON;
	ioctl(0, TCSETS, &thead);

	data = mmap(0, 	vhead.yres_virtual*fhead.line_length,
		PROT_READ | PROT_WRITE,
		MAP_SHARED, file, 0);
}

/*
exit_graphics cleans up after the graphics library by reseting the
termios settings, unmaps the fb memory, and closes the fb file.
*/
void exit_graphics(){
	ioctl(0, TCGETS, &thead);
	thead.c_lflag |= ECHO;
	thead.c_lflag |= ICANON;
	ioctl(0, TCSETS, &thead);

	munmap(data, vhead.yres_virtual*fhead.line_length);
	close(file);
}

/*
clear_screen removes all contents from the graphics screen.
*/
void clear_screen(){
	write(1,"\033[2J",4);
}

/*
get_key returns the character input y the user.
It does so without blocking by using select().

return: char of user key pressed
*/
char getkey(){
	int a;
	unsigned char b;
	int x;

	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);

	x = select(1, &fds, NULL, NULL, NULL) > 0;
	if(x) {
		if ((a = read(0, &b, sizeof(b)))< 0) {
			return a;
		} else {
			return b;
		}
	}
	return 1;
}

/*
sleep_ms sleeps the program for the given miliseconds.

arguments: long ms - miliseconds to sleep
*/
void sleep_ms(long ms){
	nanosleep(1000000 * ms, NULL);
}

/*
draw_pixel draws a pixel on the screen at a given x,y coordinate and
with a given color.

arguments: int x - x coordinate
	   int y - y coordinate
	   color_t color - 16bit rgb color value
*/
void draw_pixel(int x, int y, color_t color){
	int off = (y * vhead.xres + x)*2;
	int r = (color >> 11) & ((1 << 5) -1);
	int g = (color >> 5) & ((1 << 6) -1);
	int b = color & ((1 << 5) -1);

	if(y < vhead.yres){
		data[off + 0] = b;
		data[off + 1] = g;
		data[off + 2] = r;
	}
}

/*
draw_rect draws a rectangle on the screen with given attributes.

arguments: int x1 - top-left x-coordinate
	   int y1 - top-left y-coordinate
	   int width - width of rectangle
	   int height - height of rectangle
	   color_t c - 16bit rgb color of rectangle
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){
	int i;
	int j;
	for(i=0; i<width; i++){
		for(j=0; j<height; j++){
			draw_pixel(x1+i,y1+j,c);
		}
	}
}

/*
str_len is a helper function that returns the length of a char* string

arguments: const chat *s - string
*/
int str_len(const char *s){
	if(*s == '\0'){
		return 0;
	}
	return (1+ str_len(++s));
}

/*
draw_text draws text on the screen using the iso_font and the given
attributes.

arguments - int x - top-left x-coordinate of first letter
	    int y - top-left y-coordinate of first letter
	    const char *text - string of text to display
	    color_t c - 16bit rgb color of text
*/
void draw_text(int x, int y, const char *text, color_t c){
	int b;
	int d;
	int val;
	int pix;
	int len;
	int a;

	for(a=0; a<str_len(text);a++){
		for(b=0;b<16;b++){
			val = iso_font[text[a]*16+b];
			for(d=7;d>-0;d-=1){
				pix = (val >> d) & 1;
				if(pix != 0){
					draw_pixel((x+d)+(8*a),y+b,c);
				}
			}
		}
	}
}
