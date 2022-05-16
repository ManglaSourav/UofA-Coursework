/* 
Orlando Rodriguez 
CSC 452 Spring 2022
Library functions for:  Setting pixels to a specific colour 
	Drawing basic shapes 
	Reading keypresses 
Project 1 
*/ 
#include "library.h"
#include <sys/mman.h> 
#include <sys/time.h> 
#include <linux/fb.h> 
#include <linux/ioctl.h> 
#include <asm-generic/ioctls.h> 
#include <fcntl.h>
#include <termios.h>
#include "iso_font.h"

#define WIDTH 640
#define HEIGHT 480 
#define SIZEOF_SHORT 2

// Global variables that I absolutely needed
int fdesc;
color_t *addr;
struct fb_fix_screeninfo fScreenInfo;
struct fb_var_screeninfo vScreenInfo;
size_t length;

/* 
Initializes the graphics environment by 
retrieving information about the frame buffer, mapping it to memory
and disabling settings in stdin
*/
void init_graphics() {
	clear_screen();
	fdesc = open("/dev/fb0", O_RDWR);
	ioctl(fdesc, FBIOGET_VSCREENINFO, &vScreenInfo);
	ioctl(fdesc, FBIOGET_FSCREENINFO, &fScreenInfo);
	length = vScreenInfo.yres_virtual * fScreenInfo.line_length;
	off_t offset = 0;
	
	addr = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fdesc, offset);
	
	struct termios term0, term1;
	ioctl(0, TCGETS, &term0);
	term1 = term0;
	term1.c_lflag &= ~(ICANON | ECHO);
	ioctl(0, TCSETS, &term1);
}

/*
Exits the graphics environment
Resets stdin flags
Unmaps memory
Closes the fb
*/
void exit_graphics() {
	struct termios term0, term1;
	ioctl(0, TCGETS, &term0);
	term1 = term0;
	term1.c_lflag |= (ICANON | ECHO);
	ioctl(0, TCSETS, &term1);
	close(fdesc);
	munmap(addr, length);
}

/*
Clears the screen
*/
void clear_screen() {
	write(fdesc, "\033[2J", 4);
}

/*
Gets the key entered through stdin
*/
char getkey() {
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	// need to make a timeval struct and set its fields to 0
	struct timeval time;
	time.tv_sec = 0;
	time.tv_usec = 0;
	int retval = select(1, &rfds, NULL, NULL, &time);
	char buf;
	if (retval) {
		//write(1, "Input ENTERED", 13);
		// only have to read in one char from buffer
		read(0, &buf, 1);
		return buf;
	}
	return 0;
}

/*
Create a timespec struct and set its attributes, then call nanosleep
*/
void sleep_ms(long ms) {
	struct timespec req; 
	req.tv_sec = ms / 1000;
	req.tv_nsec = 1000000L * (ms % 1000);
	nanosleep(&req, NULL);
}

/*
Draws a pixel at a given coordinate
*/
void draw_pixel(int x, int y, color_t color) {
	// Original method of doing the coordinates, not as clean as array
	//color_t *pixel = addr + (y * fScreenInfo.line_length) + (x * SIZEOF_SHORT);
	// Want to check bounds to make sure that I'm not segfaulting
	if (y < 0)
		y = vScreenInfo.yres_virtual;
	y %= vScreenInfo.yres_virtual;
	if (x < 0)
		x = vScreenInfo.xres_virtual;
	x %= vScreenInfo.xres_virtual;
	// realized that the array method was just cleaner
	addr[(y * vScreenInfo.xres_virtual) + x] = color;
	//*pixel = color;
}

/*
Draws an outline of a rectangle at the given coordinate
Draws two parallel lines concurrently at a time
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int x, y;
	for (x = x1; x < x1 + width; x++) {
		draw_pixel(x, y1, c);
		draw_pixel(x, y1 + height, c);
	}
	for (y = y1; y < y1 + height; y++)  {
		draw_pixel(x1, y, c);
		draw_pixel(x1 + width, y, c);
	}
}

/*
Draws a letter at a given coordinate
*/
void draw_letter(int x, int y, char letter, color_t c) {
	int i;
	// Iterate across every row 
	for (i = 0; i < 16; i++) {
		// Isolate the hex row
		int row = iso_font[letter*16 + i];
		int px;
		// Iterate across bit in row
		for (px = 0; px < 8; px++) {
			// Check if the specific pixel will be coloured
			int pxIsWhite = (row >> px) & 0x01;
			if (pxIsWhite)
				draw_pixel(x + px, y + i, c);
		}
	}

}

/*
Draws text on screen by calling the draw_character method over and over again
*/
void draw_text(int x, int y, const char *text, color_t c) {
	// goes until it reaches the \0 character
	while (*text) {
		draw_letter(x, y, *text, c);
		text++;
		x += 8;
	}
}

/*
Encodes an RBG colour into a 16bit colour
*/
color_t encodeRGB(int R, int G, int B) {
	color_t colour = 0;
	// Finds the 16bit version of each component
	unsigned short colour_r = R & 0x1F;
	unsigned short colour_g = G & 0x3F;
	unsigned short colour_b = B & 0x1F;
	// Adds the components together into the new encoded colour
	colour |= colour_r << 11;
	colour |= colour_g << 5;
	colour |= colour_b; 
	return colour;
}
