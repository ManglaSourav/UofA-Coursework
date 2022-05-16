/*
Author:		Jacob Hurley
Assignment:	1
Due Date:	2/6/2022
Purpose:	Graphics library using linux base
 */
#include "iso_font.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <linux/fb.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

//declaration of header
int nanosleep(const struct timespec *req, struct timespec *rem);

typedef unsigned short color_t;

// function used to convert individual color values to 16 bit color value
int color_to_bits(int red, int green, int blue){
	return ((red<<11)|(green<<5)|(blue));
}
// variables that may be used in different functions
char *memMap;
struct termios restorTerm;
struct fb_fix_screeninfo bitDepth;
struct fb_var_screeninfo screeninfo;
int framebuffer;

// This function initializes the graphics Library
void init_graphics(){
	framebuffer = open("/dev/fb0",O_RDWR); // open framebuffer
	// get information about frameBuffer
	ioctl(framebuffer, FBIOGET_VSCREENINFO, &screeninfo);
	ioctl(framebuffer, FBIOGET_FSCREENINFO, &bitDepth);
	// get the size of the mmap array (number of bits per pixel, height, and length
	typedef size_t information;
	information info = (screeninfo.yres_virtual * bitDepth.line_length);
	
	// create a mmap variable to change pixel color values later
	memMap = mmap(NULL, info, PROT_READ | PROT_WRITE, MAP_SHARED,
	 framebuffer,0);
	
	// get the termios struct and create a copy for when function closes	
	struct termios term;
	ioctl(0, TCGETS,&term);
	restorTerm = term;

	// disable keypress echo and buffering the keypresses
	term.c_lflag &= ~(ICANON|ECHO);
	ioctl(0, TCSETS, &term);
	
}

// This function cleares the mmapd memory, closes the file buffer, and restores echo and keypress buffer.
void exit_graphics(){
	munmap(memMap, screeninfo.yres_virtual * bitDepth.line_length);
	close(framebuffer);
	ioctl(0,TCSETS,&restorTerm);
}

//This function clears the screen using an ANSI escape code
void clear_screen(){
	write(1,"\033[2J",4);
}

// This function uses the select function and read to get the key pressed by the user
char getkey(){
	fd_set rd;
	FD_ZERO(&rd);
	FD_SET(0,&rd);
	select(1,&rd,NULL,NULL,NULL);
	char c[2];
	read(0,&c,1);
	return c[0];
}

// This function takes the number of ms as input and sleeps the program for the number of ms
void sleep_ms(long ms){
	struct timespec time;
	time.tv_sec = 0;
	time.tv_nsec = ms*1000000;
	nanosleep(&time,NULL);
}

// This function will get the blue value of a 16 bit color value
int get_blue(int color){
	return (color&0x1F);
}
// This function will get the green value of a 16 bit color value
int get_green(int color){
	return ((color >> 5)&0x3F);
}
// This function will get the red value of a 16 bit color value
int get_red(int color){
	return ((color >> 11)&0x1F);
}

// This function takes a coordinate x and y, and the color desired and changes the pixel at the coordinate to the color.
void draw_pixel(int x, int y,color_t color){
	int index = ((x*2)+(y*bitDepth.line_length));
	*((unsigned short int*)(memMap+index)) = color;//use ushort because color is 16 bits long
}

// This function takes in an x1 and y1 coordinate, a width and height, and a desired color, and draws a rectangle by changing the pixel values.
void draw_rect(int x1, int y1, int width, int height,color_t c){
	int i = 0;
	while (i < width){
		int j = 0;
		while(j < height){
			draw_pixel(x1+i,y1+j,c);
			j = j+1;
		}
		i = i+1;
	}
}

// This function takes in a character and draws the character using the iso header provided
void draw_character(int x, int y, char character, color_t c){
	int i = 0;
	int ascii = character;
	while (i<16){
		int index = (ascii * 16) + i;
		int row = iso_font[index];
		int column = 0;
		while(column < 8){
			if( ((row>>column)&1) == 1){
				draw_pixel(x-7+column, y+i, c);
			}
			column = column + 1;
		}
		i = i+1;	
	}
}

//This function takes in an x and y coordinate, a const string text, and a color. It will draw the string with the given color
void draw_text(int x, int y, const char *text, color_t c){
	int i = 0;
	while (text[i] != '\0'){// for every character in a string, draw
		int xLoc = x + (i*16); // shift every character in a string right 16
		draw_character(xLoc, y, text[i], c);
		i = i+1;
	}
}
