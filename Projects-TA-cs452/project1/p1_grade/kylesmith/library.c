#include <sys/mman.h>
#include <sys/time.h>
//#include <time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <linux/fb.h>
#include "graphics.h"
#include "iso_font.h"
//void draw_char(int x, int y, char letter, color_t c);

/* Library.c
*  Author: Kyle Smith
*  Purpose: For project 1, this c program uses only system calls
*  to implement a simple graphics library.
*/

char *map;
int frameBuff;
struct termios oldTerm;
struct termios newTerm;
struct fb_var_screeninfo vscreen;
struct fb_fix_screeninfo fscreen;

/*
* Constructs a memory mapping required to create our graphics library. In
* doing this, we must pull data from our system concerning our resolution
* and framebuffer to create this map. Lastly, we turn off keyboard input
* displaying in our command line.
*/
void init_graphics() {
	frameBuff = open("/dev/fb0", O_RDWR);
 
	ioctl(frameBuff, FBIOGET_VSCREENINFO, &vscreen);
	ioctl(frameBuff, FBIOGET_FSCREENINFO, &fscreen);
	
	size_t mapSize = (vscreen.yres_virtual) * (fscreen.line_length);

	map = (char *) mmap(NULL, mapSize, PROT_WRITE, MAP_SHARED, frameBuff, 0);
	
	ioctl(0, TCGETS, &oldTerm);
	newTerm = oldTerm;
	newTerm.c_lflag &= ~ECHO;
	newTerm.c_lflag &= ~ICANON;
	ioctl(0, TCSETS, &newTerm);
		
}

/*
* Resets our display settings back to its original form/presets.
*/ 
void exit_graphics() {
	//Reenable key press
	//system("stty echo");	
	ioctl(0, TCSETS, &oldTerm);
	munmap(map,0);
	close(frameBuff);

}

/*
* Clears screen for graphics using an escape code.
*/
void clear_screen() {

	write(1, "\033[2J", 7);

}

/*
* Function that detects when there is a keystroke from the user
* then reads and returns the character.
*/
char getkey() {
	struct timeval t;
	t.tv_sec = 3;
	t.tv_usec = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	select(1, &fds, NULL, NULL, &t);
	char c;
	read(0, &c, sizeof(char));
	return c;
	

}

/*
* Basic sleep function that lets our system sleep between changes
* in our graphics, input is in milliseconds.
*/
void sleep_ms(long ms) {
	struct timespec t;
	int ms_remain = (ms) % 1000;
	long sec = ms_remain * 1000;
	
	t.tv_sec = (ms)/1000;
	t.tv_nsec = sec*1000;
	nanosleep(&t, NULL);
}

/*
* In pulling information from our screen info structs, we are able
* to calculate the x and y coordinates in our mmap and plant the
* specified color in our screens memory to be presented on the screen
*/
void draw_pixel(int x, int y, color_t color) {
	long int coord1 = x * (vscreen.bits_per_pixel/8);
	long int coord2 = y * fscreen.line_length;

        *((unsigned short int*)(map+coord1+coord2)) = color;

}

/*
* Through iterating in both of the x and y directions, we are able to
* use the draw_pixel() function to create a square to be stored in
* our framebuffer.
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int y;
	int x;
	for (y=y1; y < y1+height; y++) {
		for (x=x1; x < x1+width; x++) {
			draw_pixel(x,y,c);
		}

		
	}
}

/*
* This function allows us to print out a string with a specified color
* to our frame buffer. A bulk of the work is done by helper function
* draw_char();
*/
void draw_text(int x, int y, const char *text, color_t c) {
	int i;
	const char *letter;
	int extra = 0;
	for (letter = text; *letter != '\0'; letter++){
		draw_char(x, y+extra, *letter, c);
		extra += 8;
	}

}

/*
* This is a helper function that carries out the sub-tasks (printing
* each character) for draw_text. Via bit shifting and masking we are	
* able to decifer from our iso_font file where we should print pixels to
* our frame buffer.
*/
void draw_char(int x, int y, char letter, color_t c) {
	int i;
	int j;
	int bit;
	int val = letter;
	for (i = 0; i < 16; i++) {
		for (j=0; j < 16; j++) {
			bit = ((iso_font[val*16+i] & 1 << j) >> j);
			if (bit == 1) {
				draw_pixel(j+y, i+x, c);
			}	
		}
	}
}

