/*
Filename: library.c
Author: Ember Chan
Course: CSC 452 spr 2022
Description: Contains functions for use in graphics
*/

#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/select.h>
#include <unistd.h>
#include "graphics.h"
#include "iso_font.h"

static int term_fd;
static void* framebuffer;
static size_t fb_size;

//Initializes the library
void init_graphics(){
	int fd = open("/dev/fb0", O_RDWR); //Get file descriptor
	
	//Set up mmap
	struct fb_var_screeninfo varinfo;
	struct fb_fix_screeninfo fixinfo;
	ioctl(fd, FBIOGET_VSCREENINFO, &varinfo);
	ioctl(fd, FBIOGET_FSCREENINFO, &fixinfo);
	fb_size = varinfo.yres_virtual * fixinfo.line_length;
	framebuffer = mmap(NULL, fb_size, PROT_WRITE, MAP_SHARED, fd, 0);
	
	struct termios termy;
	term_fd = open("/dev/tty", O_RDWR);
	//Disable keypress echo and buffering
	ioctl(term_fd, TCGETS, &termy);
	termy.c_lflag &= ~ICANON; //Set ICANON to 0
	termy.c_lflag &= ~ECHO; //SEt ECHO to 0
	ioctl(term_fd, TCSETS, &termy);
}

//Unitialize/exit the graphics library
void exit_graphics(){
	//Reenable keypress echo and buffering
	struct termios termy;
	ioctl(term_fd, TCGETS, &termy);
	termy.c_lflag |= ICANON; //Set ICANON to 1
	termy.c_lflag |= ECHO; //Set ECHO to 1
	ioctl(term_fd, TCSETS, &termy);
}

//Clears the screen
void clear_screen(){
	write(term_fd, "\033[2J", 4);
}

// Returns the next character input.
// Returns 0 if the input queue is empty.
char getkey(){
	char retval = 0;
	fd_set rfds;
	FD_ZERO( &rfds);
	FD_SET(term_fd, &rfds);
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 5;
	if (select(term_fd+1,&rfds, NULL, NULL, &tv) > 0){
		read(term_fd, &retval, 1); 
	}
	return retval;
	 
}

//Sleeps for the given number of miliseconds
void sleep_ms(long ms){
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = ms*1000000;
	nanosleep(&t, NULL);
}

//Sets the pixel at x, y to the given color
void draw_pixel(int x, int y, color_t color){
	//Each pixel is 16 bits = 2 bytes
	//Screen is 640x480
	int location = y*640*2 + x*2;
	if (location + 1 < fb_size){
		*(color_t *)(framebuffer + location) = color;
	}
}

/*
Draws a rectangle of the given color. The top left corner of the
rectangle is located at (x, y). The rectangle is width wide and
height tall.
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){
	int i;
	int j;
	for(i = x1; i < x1 +width; i++){
		for(j =  y1; j < y1+height; j++){
			draw_pixel(i, j, c);
		}
	}
}

//Draws a single character at x, y
static void draw_char(int x, int y, char character, color_t c){
	int i;
	int j;
	for (i = 0; i < 8; i++){
		for(j = 0; j < 16; j++){
			if(iso_font[character*16 + j] & (1 << i)){
				draw_pixel(x+i, y+j, c);
			}
		}
	}
}

//Draws the given text at (x, y). c is the color of the text
void draw_text(int x, int y, const char *text, color_t c){
	int i = 0;
	while (*(text+i) != '\0'){
		draw_char(x + i*8 ,y, *(text+i), c);
		i++;
	}
}

