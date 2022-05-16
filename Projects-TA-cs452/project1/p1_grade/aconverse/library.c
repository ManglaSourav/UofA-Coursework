/*
 * Author: Amber Charlotte Converse
 * File: library.c
 * Description: This file implements the prototypes for a graphics library
 * 	interface for a machine running Tiny Core Linux.
 */

#define _POSIX_C_SOURCE 199309L

#include "graphics.h"
#include "iso_font.h"

#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/select.h>

int fd;
unsigned short* framebuffer = NULL;
size_t display_width = 0;
size_t display_height = 0;

/*
 * This function initializes the graphics library using the framebuffer for
 * the system.
 */
void init_graphics() {
	fd = open("/dev/fb0", O_RDWR);
	if (fd < 0) {
		const char err_msg[] = "ERROR: Framebuffer could not be "
			"accessed.\n";
		write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
		return;
	}
	struct fb_var_screeninfo virtual_screen_info;
   	struct fb_fix_screeninfo fixed_screen_info;
	if (ioctl(fd, FBIOGET_VSCREENINFO, &virtual_screen_info) ||
			ioctl(fd, FBIOGET_FSCREENINFO, &fixed_screen_info)) {
		const char err_msg[] = "ERROR: Resolution could not be "
			"obtained.\n";
		write(STDERR_FILENO, err_msg, sizeof(err_msg)-1);
		return;
	}
	display_width = fixed_screen_info.line_length;
	display_height = virtual_screen_info.yres_virtual;
	framebuffer = mmap(NULL, display_width*display_height,
			PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	
	// disable canonical mode, input echo, and cursor	
	struct termios settings;
	ioctl(0, TCGETS, &settings);
	settings.c_lflag ^= ICANON | ECHO;
	ioctl(0, TCSETS, &settings);
	write(STDOUT_FILENO, "\e[?25l", 6);
}

/*
 * This function closes out the graphics library and frees it for use by other
 * programs.
 */
void exit_graphics() {
	munmap(framebuffer, display_width*display_height);
	close(fd);
	
	// re-enable canonical mode, input echo, and cursor
	struct termios settings;
	ioctl(0, TCGETS, &settings);
	settings.c_lflag |= ICANON | ECHO;
	ioctl(0, TCSETS, &settings);
	write(STDOUT_FILENO, "\e[?25h", 6);
}

/*
 * This function clears the screen.
 */
void clear_screen() {
	write(STDOUT_FILENO, "\033[2J", 4);
}

/*
 * This function determines if a key has been pressed, and returns the key that
 * was pressed if it there was one. Otherwise, it returns -1;
 *
 * RETURN:
 * char: the key that was pressed, NULL if no key was pressed.
 */
char getkey() {
	
	fd_set s_read;
	FD_ZERO(&s_read);
	FD_SET(0, &s_read);
	
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	
	char in = -1;
	if (select(1, &s_read, NULL, NULL, &timeout)) {
		char strIn[2];
		read(STDIN_FILENO, strIn, 1);
		in = strIn[0];
	}
	return in;	
}

/*
 * This function pauses the program for the given amount of milliseconds.
 *
 * PARAM:
 * ms (long): the number of milliseconds to pause.
 */
void sleep_ms(long ms) {
	struct timespec time;
	time.tv_sec = ms / 1000;
	time.tv_nsec = (ms % 1000) * 1000000;
	nanosleep(&time, NULL);
}

/*
 * This function changes the color of the pixel at (x,y) to the given color.
 *
 * PARAM:
 * x (int): the x-coordinate of the target pixel
 * y (int): the y-coordinate of the target pixel
 * color (color_t): the color to change the target pixel to (color_t type
 * 	defined above)
 */
void draw_pixel(int x, int y, color_t color) {
	if (x < display_width/2 && y < display_height) { 
		framebuffer[(y*display_width)/2 + x] = color;
	}
}

/*
 * This function draws a rectangle with corners (x1,y1), (x1+width,y1),
 * (x1+width,y1+height), (x1,y1+height) with the given color.
 *
 * PARAM:
 * x1 (int): the x-coordinate of the base corner
 * y1 (int): the y-coordinate of the base corner
 * width (int): the width of the rectangle, extruded from the base corner
 * height (int): the height of the rectangle, extruded from the base corner
 * c (color_t): the color of the rectangle
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int x2 = x1+width; int y2 = y1+height;
	if (x2 < x1) { // make sure x1 is min 
		int temp = x1;
		x1 = x2;
		x2 = temp;
	}
	if (y2 < y1) { // make sure y1 is min
		int temp = y1;
		y1 = y2;
		y2 = temp;
	}
	for (int cur_y = y1; cur_y < y2; cur_y++) {
		for (int cur_x = x1; cur_x < x2; cur_x++) {
			draw_pixel(cur_x, cur_y, c);
		}
	}
}

/*
 * This function draws the given string with the specified color at the
 * starting location (x,y).
 *
 * PARAM:
 * x (int): the x-coordinate of the upper left corner of the first letter
 * y (int): the y-coordinate of the upper left corner of the first letter
 * text (const char*): the string to be drawn (will not be changed)
 * c (color_t): the color of the text
 */
void draw_text(int x, int y, const char *text, color_t c) {
	for (const char* cur = text; *cur; cur++) {
		for (int row_index = 0; row_index < 16; row_index++) {
			char row = iso_font[(*cur)*16+row_index];
			for (int col_index = 0; col_index < 8; col_index++) {
				char cur_pixel = 
					(row & (0x1 << (col_index)))
					>> (col_index); // mask for cur bit
				if (cur_pixel == 1) {
					draw_pixel(x+col_index,y+row_index,c);
				}
			}
		}
		x += 8;	
	}
}
