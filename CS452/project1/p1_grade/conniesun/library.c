#include "graphics.h"
#include "iso_font.h"

// do not use C standard library
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <time.h>

/*
 * library.c
 *
 * Author: Connie Sun
 * Course: CSC 452 Spring 2022
 * 
 * A graphics library that can draw text, rectangles, and pixels
 * to the frame buffer graphics device. Can also read key press
 * input (one char at a time) and sleep for a specified number of
 * ms. Uses system calls instead of C standard library.
 *
 */

// global variables
int fb_filedes;
int filesize;
int line_length;
void *address;

/*
 * initialize graphics by 
 * 1. opening the graphics device (frame buffer)
 * 2. getting information about the screen
 * 3. mapping the memory of the frame buffer file
 * 4. disabling keypress echo and buffering
 * 5. calling clear_screen()
 */
void init_graphics() {
	// open the graphics device
	fb_filedes = open("/dev/fb0", O_RDWR);
	// get information about the screen
	struct fb_var_screeninfo fb_var;
	struct fb_fix_screeninfo fb_fix;
	ioctl(fb_filedes, FBIOGET_VSCREENINFO, &fb_var);
	ioctl(fb_filedes, FBIOGET_FSCREENINFO, &fb_fix);
	filesize = fb_var.yres_virtual * fb_fix.line_length;
	line_length = fb_fix.line_length;
	// memory mapping
	address = mmap(NULL, filesize, PROT_READ | PROT_WRITE, 
	MAP_SHARED, fb_filedes, 0);
	if (address == MAP_FAILED) {
		const char *msg = "mmap failed\n";
		write(STDOUT_FILENO, msg, sizeof(msg) - 1);
	}
	// disable keypress echo and buffering
	struct termios t;
	ioctl(STDIN_FILENO, TCGETS, &t);
	t.c_lflag &= ~(ICANON | ECHO);
	ioctl(STDIN_FILENO, TCSETS, &t);
	// clear the screen
	clear_screen();
}

/*
 * clean up by clearing screen, closing file, unmapping memory,
 * and reenabling key press echo and buffering
 */
void exit_graphics() {
	clear_screen();
	// close and unmap memory
	close(fb_filedes);
	munmap(address, filesize);
	// reenable key press echo and buffering
	struct termios t;
	ioctl(STDIN_FILENO, TCGETS, &t);
	t.c_lflag |= (ICANON | ECHO);
	ioctl(STDIN_FILENO, TCSETS, &t);
}

/*
 * clear the screen by writing an escape sequence to std out
 * same as typing "Ctrl + L"
 */
void clear_screen() {
	write(STDOUT_FILENO, "\033[2J", 4);
}

/*
 * read key press input using select() and read().
 * select() is non-blocking, so we can check if there
 * is any key press at all; then, we read it
 */
char getkey() {
	int ret;
	char buf[1];
	fd_set readfd;
	FD_ZERO(&readfd);
	FD_SET(STDIN_FILENO, &readfd);
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	ret = select(STDIN_FILENO + 1, &readfd, NULL, NULL, &timeout);
	if (ret) {
		read(STDIN_FILENO, buf, 1);
		return buf[0];
	} return '\0';
}

/*
 * wait the specified number of milliseconds 
 */
void sleep_ms(long ms) {
	struct timespec sleep_time;
	sleep_time.tv_sec = 0;
	sleep_time.tv_nsec = ms * 1000000;
	nanosleep(&sleep_time, NULL);
}

/*
 * use coordinates to scale address of mmap'ed framebuffer
 * frame buffer is row-major order
 */
void draw_pixel(int x, int y, color_t color) {
	// width is 640, height is 480, with 16-bit color
	if (x >= 639 || x < 0 || y >= 479 || y < 0) return;
	color_t *fb_address = (color_t *)address;
	fb_address[y * 640 + x] = color;
}

/*
 * draw a rectangle to the screen with top left corner at x1,y1
 * and the specified width and height. calls draw_pixel() to 
 * color in the rectangle
 */
void draw_rect(int x1, int y1, int width, int height, color_t color){
	int i, j;
	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			draw_pixel(x1 + j, y1 + i, color); // draw in row order
		}
	}
}

/*
 * draw text to the screen by calling draw_char(). iterate until
 * the null character is found
 */
void draw_text(int x, int y, const char *text, color_t color) {
	while (*text != '\0') {
		draw_char(x, y, *text, color);
		x += 8; // each letter is width 8 pixels
		text++;
	}
}

/*
 * draw a single character to the screen, where x,y is the top 
 * left corner. each character is width 8 and height 16. call 
 * draw_pixel() for each bit of the character that is "on"
 */ 
void draw_char(int x, int y, char c, color_t color) {
	int i, j, draw;
	unsigned char row; // a single byte 
	for (i = 0; i < 16; i++) {
		row = iso_font[c * 16 + i];
		// now shift and mask each bit and draw pixel if bit is 1
		for (j = 0; j <= 7; j++) {
			draw = 1 & row;
			if (draw == 1) // if bit is 1, draw at x + j, y + i
				draw_pixel(x + j, y + i, color);
			row = row >> 1;
		}
	}
}

// used just to get colors from macros
color_t get_color_from_rgb(color_t r, color_t g, color_t b) {
	color_t ret = 0;
	ret += r << 11;
	ret += g << 5;
	ret += b;
	return ret;
}
