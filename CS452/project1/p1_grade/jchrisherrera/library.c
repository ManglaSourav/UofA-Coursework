#include<sys/time.h>
#include<time.h>
#include<sys/select.h>
#include<linux/fb.h>
#include<sys/ioctl.h>
#include<sys/mman.h>
#include<sys/fcntl.h>
#include<sys/unistd.h>
#include<termios.h>
#include "iso_font.h"
#include "graphics.h"

/*
 * Author: Chris Herrera
 * CSC452
 * Project 1
 */

int fileDesc;	// file descriptor for the frame buffer
struct termios prevTerminal, lockedTerminal;	// for manipulation of keypress echo & buffering
color_t *map;	// pointer to traverse the mapped frame buffer
size_t size;	// used to store the total size of the mapped frame buffer
int xresvirt;  // used to store xres_virtual for use in draw_pixel


// Clear the screen by writing the clear ANSI escape code to STDOUT
void clear_screen() {
	char clear[] = "\033[2J";
	write(1, clear, 4);
}


/*
 * Initialize our graphics library by mapping the video display's framebuffer 
 * and disabling keypress echo & buffering
 */
void init_graphics() {
	clear_screen();
	struct fb_var_screeninfo varScreenInfo;
	struct fb_fix_screeninfo fixScreenInfo;

	// Load the file descriptor of the frame buffer
	fileDesc = open("/dev/fb0", O_RDWR);
	ioctl(fileDesc, FBIOGET_VSCREENINFO, &varScreenInfo);
	ioctl(fileDesc, FBIOGET_FSCREENINFO, &fixScreenInfo);
	
	// Get screen info
	int ypixels = varScreenInfo.yres_virtual; // pixels
	int lineBytes = fixScreenInfo.line_length; // bytes
	xresvirt = varScreenInfo.xres_virtual; // pixels
	
	// Determine map size and map the frame buffer
	size = ypixels*lineBytes;
	map = mmap(NULL, ypixels*lineBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fileDesc, 0);

	// Get a reference to current keypress echo & buffer settings to revert back upon exit
	ioctl(0, TCGETS, &prevTerminal);

	// Make a copy of the keypress echo & buffer settings, and change the ECHO and ICANON bits
	// to disable keypress echo and buffering
	lockedTerminal = prevTerminal;
	lockedTerminal.c_lflag &= ~(ECHO | ICANON);
	ioctl(0, TCSETS, &lockedTerminal);

}


/*
 * Draw a single pixel at coordinates x, y of color color.  Out of bounds segfaults are prevented
 */
void draw_pixel(int x, int y, color_t color) {
	if (((xresvirt*y)+x >= size/2) | ((xresvirt*y)+x < 0)) return;
	else map[(xresvirt*y)+x] = color;
}


/*
 * Clean up by resetting keypress echo & buffer, unmapping the frame buffer, and closing the file descriptor
 */
void exit_graphics() {
	ioctl(0, TCSETS, &prevTerminal);
	munmap(map, size);
	close(fileDesc);
}


/*
 * Get a keypress from Standard In
 */
char getkey() {
	struct timeval time;
	char c;
	time.tv_sec = 0;	// Prevent blocking by setting timeout to 0
	time.tv_usec = 0;
	fd_set rdInput;		// Create a fd_set for Standard In
	FD_SET(STDIN_FILENO, &rdInput);		// Set up for STDIN since that's where our chars will be entered
	if (select(STDIN_FILENO+1, &rdInput, NULL, NULL, &time)) {	// If a char is entered, load into c and return
		read(0, &c, 1);
	}
	return c;
}


// Sleep for ms milliseconds
void sleep_ms(long ms) {
	struct timespec time;
	time.tv_sec = 0;
	time.tv_nsec = ms*1000000;
	nanosleep(&time, NULL);
}

/*
 * Make a rectangle with corners (x1,y1), (x1+width,y1), (x1+width,y1+height), (x1,y1+height) and color c
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int x;
	int y;
	for (x = x1; x <= x1+width; x++) {
		draw_pixel(x,y1,c);
		draw_pixel(x,y1+height,c);
	}
	for (y = y1; y <= y1+height; y++) {
		draw_pixel(x1,y,c);
		draw_pixel(x1+width,y,c);
	}
}


/*
 * Draw a string with the specified color at starting location (x,y)
 */
void draw_text(int x, int y, const char *text, color_t c) {
	int i = 0;
	char ch = text[i];
	while (ch != '\0') {
		draw_letter(ch, x, y, c);
		i++;
		ch = text[i];
		x += 8;
	}
}


/*
 * Helper function for draw_text.  Draw a single char starting at (x, y) of color c.
 */
void draw_letter(char ch, int x, int y, color_t c) {
	int ascii = ch;
	int i;
	int savex = x; // save initial x to reset after drawing for each 1-byte integer
	for (i = 0; i<16; i++) {
		int byteInt = iso_font[(ascii*16)+i]; // Index into the font array to the 16 1-byte integers
		
		int bit;	// Starting with the LSB, use masking to check each position of byteInt
		for (bit = 1; bit < 256; bit = bit*2) {
			if (byteInt & bit) {
				draw_pixel(x,y,c);	// If a position in the byte is 1, we draw a pixel
			}
			x++;
		}
		y++; // Move down
		x = savex; // Reset x to continue drawing
	}
}


/*
 * This is a special effect for my custom driver.  It draws a single character but slowly
 */
void slow_write(char ch, int x, int y, color_t c) {
	int ascii = ch;
	int i;
	int savex = x; // save initial x to reset after drawing for each 1-byte integer
	for (i = 0; i<16; i++) {
		int byteInt = iso_font[(ascii*16)+i]; // Index into the font array to the 16 1-byte integers
		
		int bit;	// Starting with the LSB, use masking to check each position of byteInt
		for (bit = 1; bit < 256; bit = bit*2) {
			if (byteInt & bit) {
				sleep_ms(1); // Slowly draw
				draw_pixel(x,y,c);	// If a position in the byte is 1, we draw a pixel
			}
			x++;
		}
		y++; // Move down
		x = savex; // Reset x to continue drawing
	}
}



