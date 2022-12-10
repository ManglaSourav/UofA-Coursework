/**
 * File Name: library.c
 * Author: Michael Tuohy
 * Class: CSc 452
 * NetID: michaeltuohy@email.arizona.edu
 * Description: This project is a test of the graphics capabilities of 
 * Linux, using only system calls to directly manipulate the framebuffer.
 * It also provides some basic means of drawing on the screen and reading
 * user keypresses live in order to provide a library for other programs
 * to use.  
 **/ 

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <linux/fb.h>
#include <termios.h>
#include <errno.h>
#include "iso_font.h"

#define MASK 0x01

typedef unsigned short color_t;

color_t make_color(unsigned short red, unsigned short green, unsigned short blue);

int frame_fd, terminal_fd;
unsigned short *framebuffer;
unsigned long fb_size;
struct termios previous_settings, new_settings;

unsigned long x_res;
unsigned long y_res;
unsigned long bit_def;


/*
 * This is a method that always needs to be called first, as it sets up the library
 * for use. It maps the framebuffer to framebuffer, a pointer that's available to the
 * entire library. It also disables keypress echo, so that users don't see their
 * keypresses when utilizing a program. Note that in order to restore these settings,
 * you must call exit_graphics().
 */
void init_graphics() {
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	frame_fd = open("/dev/fb0", O_RDWR);
	
	// If frame_fd == -1, then opening of the FrameBuffer has failed
	if(frame_fd == -1) {
		// printf("Error: framebuffer device not opened\n");
		// exit(1);
	}
	
	// If this syscall returns -1, then getting the vscreen_info has failed
	if(ioctl(frame_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		// printf("Error reading vscreeninfo, terminating\n");
		// exit(1);
	}

	// If this syscall returns -1, then getting the fscreen_info has failed
	if(ioctl(frame_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		// printf("Error reading fscreeninfo, terminating\n");
		// exit(1);
	}
	x_res = vinfo.xres_virtual;
	y_res = vinfo.yres_virtual;
	bit_def = finfo.line_length;
	
	//	printf("x_res: %ld, y_res: %ld, bit_def: %ld\n", y_res, bit_def);
		
	fb_size = y_res * bit_def;

	framebuffer = mmap(NULL, fb_size, (PROT_READ | PROT_WRITE), MAP_SHARED, frame_fd, 0);
	
	// Check to see if the mapping has failed
	if(framebuffer == MAP_FAILED) {
		// printf("Mapping of Framebuffer failed, terminating\n");
		// exit(1);
	}

	// Now we need to disable keypress echo, so that when we type, the letters don't appear on screen 
	// This means that we need to open the terminal settings, located at /dev/ttyS0, in order ot maniuplate
	// the termios struct

	tcgetattr(STDIN_FILENO, &previous_settings);
	new_settings = previous_settings;
	new_settings.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);

	// If at this point, we have successfully mapped the frame buffer and are ready to start manipulating the screen

	// printf("Successfully set up graphics\n");
}

/*
 * This method cleans up the program and restores the system settings to match what was changed in 
 * init_graphics(). It also unmaps the Framebuffer and closes the file descriptor, which isn't really
 * necessary, but it's good style.
 */
void exit_graphics() {

	// Most importantly, we need to reenable keyboard press echo. Since we saved the old settings to old_settings, 
	// we should be able to just do an ioctl system call with old_settings

	tcsetattr(STDIN_FILENO, TCSANOW, &previous_settings);

	// If at this point, we have successfully enabled keypress echo, and are ready to terminate

	if(munmap(framebuffer, fb_size) == -1) {
		// printf("exit_graphics: Framebuffer unsuccessfully unmapped\n");
	}

	if(close(frame_fd) == -1) {
		// printf("exit_graphics: close failed\n");
	}

	// If at this point, everything has closed successfully, and we are ready to terminate
	
	// printf("Successfully reenabled Keypress echo\n");

}


/*
 * This method calls the "write" system call in order to "print" the ansi escape sequence for
 * putting a large black rectangle on the screen, allowing us to clear the screen 
 */
void clear_screen() {
	char escape[] = "\033[2J";

	//printf("With any luck, this should disappear");

	write(STDOUT_FILENO, escape, sizeof(escape) * sizeof(escape[0]));
}

/*
 * This method reads the users keypresses as they are happening. It does this by
 * checking to see if there are any keypresses stored in stdin, then reads that character
 * and returns it, so we don't have to wait on the user to flush stdin by pressing 'Enter'
 */
char getkey() {

	// First, we have to set up select in order to read keypress info, then we can read the keypress

	char c;
	struct timeval tv;
	fd_set fs;
	tv.tv_usec = tv.tv_sec = 0;

	// Now we're prepping select to look at StandardIn to see if there is anything inside the buffer

	FD_ZERO(&fs);
	FD_SET(STDIN_FILENO, &fs);
	select(STDIN_FILENO + 1, &fs, 0, 0, &tv);


	// Now Select is set to read a keyboard press, should we want one, and we do this by calling read
	if(FD_ISSET(STDIN_FILENO, &fs)) {
		read(STDIN_FILENO, &c, sizeof(c));
	}

	return c;

}

/*
 * This method takes a long representing the milliseconds you would like the process to sleep,
 * then calls the nanosleep syscall in order to make the process sleep, allowing the user
 * to stop the program for timing purposes
 */
void sleep_ms(long ms) {
	struct timespec spec;
	spec.tv_sec = 0;
	spec.tv_nsec = ms * 1000000;

	if(nanosleep(&spec, NULL) != 0) {
		// printf("Error in nanosleep\n");
	}

}

/*
 * This method takes three color arguments, and will rearrange them such that the
 * color will match the values, returning it as a color_t. This allows users
 * to easily mix colors, should they want to
 */
color_t make_color(unsigned short red, unsigned short green, unsigned short blue) {
	color_t output = 0;
	red = red << 11;
	green = green << 5;
	
	output = red | green | blue;
	return output;
}

/* 
 * This method takes two integers representing where on the screen you would like to
 * edit one specific pixel, as well as a color, then edits the pixel on screen
 */
void draw_pixel(int x, int y, color_t color) {
	// First, we need to move the pointer so that it is in the correct row. We can do this by multiplying y
	// by the horizontal resolution of the screen, which we got in init_graphics.

	// Basic error checking
	if(x < 0 || x >= x_res) { return; }
	if(y < 0 || y >= y_res) { return; }

	int offset;
	unsigned short *pixel_to_change = framebuffer;

	offset = y * x_res;

	// Next, we access the value at x, and set it to the color that we want

	offset = offset + x;

	// Now we just edit that pixel


	pixel_to_change += offset;

	*pixel_to_change = color;

	// Pixel should be changed
	
}

/*
 * This method draws a rectangle starting at x1, y1, of the requested width, height and color
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	
	// Now, we just call draw_pixel a bunch of time

	int x_offset, y_offset;

	for(x_offset = 0; x_offset < width; x_offset++) {
		for(y_offset = 0; y_offset < height; y_offset++) {
			draw_pixel(x1 + x_offset, y1 + y_offset, c);
		}
	}

}

/*
 * This method draws one character to the screen. The character sheet is included in the file,
 * and each character is 8*16 bits corresponding to which pixels to turn on. We use bit shifting
 * and masks in order to turn on pixels individually in order to display a character properly
 */
void draw_char(int x, int y, char letter, color_t c) {
	int x_offset, y_offset;
	int character_location = letter * 16;
	unsigned char row, pixel;	

	for(y_offset = 0; y_offset < 16; y_offset++) {
		row = iso_font[character_location + y_offset];
		if(row == 0x00) {
			// If the row is blank, we don't need to do anything, so we just continue
			continue;
		}
		
		// At this point, we know that we need to draw at least one pixel, so we need 
		// To iterate upon the row bits in order to turn on the pixels that need to be lit up

		for(x_offset = 0; x_offset < 8; x_offset++) {
			pixel = row;
			pixel = pixel >> (8 - (x_offset+1));
			pixel &= MASK;
	
			if(pixel) {
				draw_pixel((x+8) - x_offset, y + y_offset, c);
			}
		}


		// At this point
	}
}

/* 
 * This method takes an array of chars that you wish to display on screen,
 * and determines where to place the characters properly so that it
 * looks like a word
 */
void draw_text(int x, int y, const char *text, color_t c) {
	int index;
	char letter, continue_looping;

	index = 0;
	continue_looping = 1;

	while(continue_looping) {
		letter = text[index];
		// Check to see if we can stop
		if(letter == '\0') {
			continue_looping = 0;
		} else {
			draw_char(x + (index * 8), y, letter, c);
			index++;
		}
	}

}
