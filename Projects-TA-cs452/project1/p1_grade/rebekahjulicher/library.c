/*
 * File: library.c
 * Author: Rebekah Julicher
 * Purpose: Creates a graphics library using linux syscalls
*/

#include <sys/select.h>
#include <unistd.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "iso_font.h"

int fd = 0;
int screenSize;
struct fb_var_screeninfo vScreen;
struct fb_fix_screeninfo fScreen;

typedef unsigned short color_t;

color_t *screen;

// clear_screen() - clears the screen of graphics
void clear_screen(){
        write(1, "\033[2J", 4);
}

// init_graphics() - initializes graphics library
void init_graphics(){
	fd = open("/dev/fb0", O_RDWR);
	// Set screen resolution variables
	
	ioctl(fd, FBIOGET_VSCREENINFO, &vScreen);
	ioctl(fd, FBIOGET_FSCREENINFO, &fScreen);

	screenSize = vScreen.yres_virtual * fScreen.line_length;
	// Handle mapping
	
	screen = (unsigned short *) mmap(NULL, screenSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	// Set terminal bits
	
	struct termios terminal;
	ioctl(0, TCGETS, &terminal);
	terminal.c_lflag &= ~(ICANON | ECHO);
	ioctl(0, TCSETS, &terminal);

	clear_screen();
}

// exit_graphics() - cleans up after graphics library
void exit_graphics(){
	clear_screen();
	close(fd);
	munmap(screen, screenSize);
	// Reset terminal bits;
	
	struct termios terminal;
        ioctl(0, TCGETS, &terminal);
        terminal.c_lflag |= (ICANON | ECHO);
	ioctl(0, TCSETS, &terminal);
}

// getkey() - reads a key input from user
char getkey(){
	fd_set set;
	struct timeval timeout;
	timeout.tv_sec  = 0;
	timeout.tv_usec = 0;

	FD_ZERO(&set);
	FD_SET(0, &set);
	
	int check = select(1, &set, NULL, NULL, &timeout);
	if (check > 0){
		char key;
		read(0, &key, 1);
		return key;
	}
	return 0;
}

// sleep_ms(int) - makes program sleep for a given number of milliseconds
void sleep_ms(int num){
	struct timespec time;
	if (num > 1000){
		time.tv_sec = num/1000;
		time.tv_nsec = (num % 1000) * 1000000;
	}
	else {
		time.tv_sec = 0;
		time.tv_nsec = num * 1000000;
	}
	nanosleep(time, NULL);
}

// draw_pixel(int, int, color_t) - draws a pixel at a given coordinate onscreen
void draw_pixel(int x, int y, color_t c){
	if (x < vScreen.xres && y < vScreen.yres){
		screen[(vScreen.xres_virtual*y) + x] = c;
	}
}

// draw_rect(int, int, int, int, color_t) - draws a rectangle at given coordinates onscreen
void draw_rect(int x1, int y1, int width, int height, color_t c){
	int i = y1;
	while (i <= (y1 + height)){
		draw_pixel(x1, i, c);
		draw_pixel(x1 + height, i, c);
		i++;
	}
	i = x1;
	while (i <= (x1 + width)){
		draw_pixel(i, y1, c);
		draw_pixel(i, y1 + height, c);
		i++;
	}
}

// fill_rect(int, int, int, int, color_t) - draws a filled rectangle at given coordinates
void fill_rect(int x1, int y1, int width, int height, color_t c){
	int i = y1;
	while (i <= (y1 + height)){
		int j = x1;
		while (j <= (x1 + width)){
			draw_pixel(j, i, c);
			j++;
		}
		i++;
	}
}

// draw_char(int, int, char, color_t) - draws a character at given coordinates onscreen
void draw_char(int x, int y, char letter, color_t c){
	int i = 0;
	while (i <= 15){
		int curr_line = iso_font[(letter * 16) + i];
		int j = 0;
		while (j <= 7){
			int curr_pixel = (curr_line >> j) & 1;
			if (curr_pixel){
				draw_pixel(x + j, y + i, c);
			}
			j++;
		}
		i++;
	}
}

// draw_text(int, int, const char, color_t) - draws text at given coordinates onscreen
void draw_text(int x, int y, const char *text, color_t c){
	int i = 0;
	int tempX = x;
	while (text[i] != '\0'){
		draw_char(tempX, y, text[i], c);
		i++;
		tempX += 8;
	}
}

