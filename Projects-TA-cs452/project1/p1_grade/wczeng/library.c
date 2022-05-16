/*
 * Author: Winston Zeng
 * File: library.c
 * Class: CSC 452, Spring 2022
 * Project 1: Graphics Library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <assert.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/fb.h>
#include "iso_font.h"
#include "library.h"

int fd0;
int result;
fd_set readset;
size_t size;
size_t horizontal;
size_t vertical;
struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;
typedef unsigned short color_t;
color_t *address;
struct termios old, new;

/*
 * Function: clear_screen()
 * Purpose: Uses an ANSI escape code to tell the terminal
 * to clear itself.
 *
 * @param: None
 * @return: None
 */
void clear_screen() {
    write(1, "\033[2J", 4);
}

/*
 * Function: init_graphics()
 * Purpose: Initializes the graphics library by:
 * 1. opening a framebuffer
 * 2. memory mapping the framebuffer to manipulate its contents
 * 3. Using ioctl to obtain the dimensions of the screen
 * 4. Disabling keypress echo
 *
 * @param: None
 * @return: None
 */
void init_graphics() {
    fd0 = open("/dev/fb0", O_RDWR);
    if(fd0 == -1) {
        perror("Error opening /dev/fb0");
        exit(1);
    }
    if(!ioctl(fd0, FBIOGET_VSCREENINFO, &var)) {
        vertical = var.yres_virtual;
    }

    if(!ioctl(fd0, FBIOGET_FSCREENINFO, &fix)) {
    	horizontal = (fix.line_length/2);
    }
    
    size = horizontal * vertical * 2;
    off_t t = 0;    
    address = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd0, t);
    if(address == (void *) -1) {
    	perror("Error mapping memory");
    	exit(1);
    }

    ioctl(STDIN_FILENO, TCGETS, &old);
    new = old;
    new.c_lflag &= ~(ICANON | ECHO);
    ioctl(STDIN_FILENO, TCSETS, &new);
    
    clear_screen();
}

/*
 * Function: exit_graphics()
 * Purpose: The clean-up function: reenables keypress
 * echoing and buffering, clears the screen, unmaps
 * memory for framebuffer, closes framebuffer.
 *
 * @param: None
 * @return: None
 */
void exit_graphics() {
    int fd = 0;
    ioctl(fd, TCSETS, &old);
    clear_screen();
    
    if(munmap(address, size) == -1) {
    	perror("Error unmapping memory");
    	exit(1);
    }
    
    if(!close(fd0)) {
    	exit(0);
    } else {
    	perror("Error closing /dev/fb0");
    	exit(1);
    }
}

/*
 * Function: getkey()
 * Purpose: Reads single character keypress inputs
 * using the Linux non-blocking system call select().
 *
 * @param: None
 * @return: keypress - character typed from keyboard
 */
char getkey() {
    FD_ZERO(&readset);
    FD_SET(STDIN_FILENO, &readset);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int retval;
    retval = select(STDIN_FILENO+1, &readset, NULL, NULL, &tv);
    if(retval == 1) {
    	char keypress;
    	read(0, &keypress, 1);
    	return keypress;
    } else {
    	return 0;
    }
}

/*
 * Function: sleep_ms()
 * Purpose: Makes program sleep b/w frames of graphics being
 * drawn. Uses system call nanosleep(), but does not require
 * nano-level granularity.
 *
 * @param: ms - amount of time to sleep in milliseconds
 * @return: None
 */
void sleep_ms(long ms) {
    struct timespec time;
    time.tv_sec = ms / 1000;
    time.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&time, NULL);
}

/*
 * Function: draw_pixel()
 * Purpose: Main drawing function, sets the pixel at coordinate
 * (x,y) to the specified color. Uses given coordinates to scale
 * the base address of the memory-mapped framebuffer using
 * pointer arithmetic.
 *
 * @param: x - x-coordinate of desired pixel
 * @param: y - y-coordinate of desired pixel
 * @param: color - desired color represented as unsigned 16 bit int
 * @return: None
 */
void draw_pixel(int x, int y, color_t color) {
    if(y >= 0 && y < 480) {
    	address[var.xres_virtual * y + x] = color;
    }
}

/*
 * Function: draw_rect()
 * Purpose: Using draw_pixel, draws a rectangle with upper left
 * corner at given coordinates, with the given dimensions.
 *
 * @param: x1 - x-coordinate of upper lefthand coordinate of rect
 * @param: y1 - y-coordinate of upper lefthand coordinate of rect
 * @param: width - width of rectangle in pixels
 * @param: height - height of rectangle in pixels
 * @param: c - desired color represented as unsigned 16 bit int
 * @return: None
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    int i;
    for(i = 0; i < width; i++) {
    	draw_pixel(x1+i, y1, c);
    	draw_pixel(x1+i, y1+height, c);
    }
    int j;
    for(j = 0; j < height; j++) {
    	draw_pixel(x1, y1+j, c);
    	draw_pixel(x1+width, y1+j, c);
    }
}

/*
 * Function: fill_rect()
 * Purpose: draws a full rectangle, as opposed to draw_rect()
 * which only draws the outline of a rectangle.
 */
void fill_rect(int x1, int y1, int width, int height, color_t c) {
    int i;
    for(i = 0; i < width; i++) {
    	int j;
    	for(j = 0; j < height; j++) {
    	    draw_pixel(x1+i, y1+j, c);
    	}
    }
}

/*
 * Function: draw_letter()
 * Purpose: Helper function, draws a single letter encoded into
 * an array.
 *
 * @param: x - x-coordinate of upper lefthand corner of letter
 * @param: y - y-coordinate of upper lefthand corner of letter
 * @param: *letter - pointer to single character
 * @param: c - 16 bit color
 * @return: None
 */
void draw_letter(int x, int y, const char *letter, color_t c) {
    int ascii = (int) *letter;			// convert character to ascii
    int i;
    for(i=0; i<16; i++) {			// iterate over letter height of 16
    	int value = iso_font[ascii*16+i];	// index into iso_font array
    	int j;
    	for(j=7; j>=0; j--) {
    	    int power = 1;
    	    int k;
    	    for(k=0; k<j; k++) {
    	        power = power * 2;		// calculate mask
    	    }
    	    int temp = value & power;		// mask
    	    int bit = temp >> j;		// and shift
    	    if(bit == 1) {
    	        draw_pixel(x+j, y+i, c);	// draw if bit is 1
    	    }
    	}
    }
}

/*
 * Function: draw_text()
 * Purpose: Using draw_letter(), draws the specified string with
 * the specified color at the starting location (x,y) - upper left
 * corner of first letter, using font encoded into an array from
 * iso_font.h.
 *
 * @param: x - x-coordinate of upper lefthand corner of first letter
 * @param: y - y-coordinate of upper lefthand corner of first letter
 * @param: *text - pointer to string
 * @param: c - the color
 * @return: None
 */
void draw_text(int x, int y, const char *text, color_t c) {
    const char *ptr = text;
    while (*ptr != '\0') {
    	draw_letter(x, y, ptr, c);
    	x = x+8;
    	ptr++;
    }
}
