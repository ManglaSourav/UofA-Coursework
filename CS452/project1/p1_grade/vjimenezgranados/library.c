/*
* File: library.c
* Purpose: Implements several graphic methods of our library.
*
* Author: Victor A. Jimenez Granados
* Date: Feb 04, 2022
*/

#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/select.h>
#include "graphics.h"
#include "iso_font.h"

#define FBDEV "/dev/fb0"
#define ONE_MIL 1000000
void draw_letter(int x, int y, char c, color_t color);

struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
struct termios oldT, newT;
int pixelWidth, pixelHeight, fBuff, screenSize, writingSpot;
char* memBlock;

void init_graphics() {
	pixelWidth = pixelHeight = fBuff = screenSize = 0;
	//Opens the FrameBuffer.
	fBuff = open(FBDEV, O_RDWR);
	pixelWidth = 640;
	pixelHeight = 480;
	if (fBuff >= 0) {
		//If the framebuffer is open, get screen information.
		if (ioctl(fBuff, FBIOGET_VSCREENINFO, &vinfo) != -1 && ioctl(fBuff, FBIOGET_FSCREENINFO, &finfo) != -1) {
			screenSize = vinfo.yres_virtual * finfo.line_length;
			//Allocate a chunk of memory of the frameBuffer.
			memBlock = (char*) mmap(0, screenSize, PROT_READ | PROT_WRITE, MAP_SHARED, fBuff, 0);
			//Save old settings and disable ECHO/ICANON.
			ioctl(0, TCGETS, &oldT);
			newT = oldT;
			newT.c_lflag &= ~ICANON; 
			newT.c_lflag &= ~ECHO; 
			ioctl(0, TCSETS, &newT);
		}
	}
}

void exit_graphics() {
	clear_screen();
	//Unmaps and clears memory and reenables ECHO/ICANON.
	munmap((int*)fBuff, screenSize);
	ioctl(0, TCSETS, &oldT);
	close(fBuff);
}

void clear_screen() {
	write(1, "\033[2J", 4);
}

char getkey() {
	fd_set rfds;
	int retVal;

	/* Watch stdin (fd 0) to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	retVal = select(1, &rfds, 0, 0, NULL);

	//Waits for Key Press to Start.
	if (retVal) {
		char key;
		read(0, &key, 1);
		return key;
	}
	return '\0';
}

void sleep_ms(long ms) {
	//Sleeps for the specified time in ms.
	struct timespec request = { 0, ms * ONE_MIL };
	nanosleep(&request, NULL);
}

void draw_pixel(int x, int y, color_t color) {
	//Finds the location of the pixel based on the screen.
	int location = (x * vinfo.bits_per_pixel / 8) + y * finfo.line_length;
	//Offsets the memory allocated of the buffer and changes the color of the pixel.
	*((unsigned short int*)(memBlock + location)) = color;
}

void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int x = x1; 
	int y = y1;
	//Iterates through a rectangle's width and height and draws a pixel to make a rectangle.
	for (y = y1; y <= y1 + height; y++) {
		for (x = x1; x <= x1 + width; x++) {
			if (y == y1 || y == y1 + height) {
				draw_pixel(x, y, c);
			}
			else if (x == x1 || x == x1 + width) {
				draw_pixel(x, y, c);
			}
		}
	}
}

void draw_text(int x, int y, const char* text, color_t c) {
	int i = 0;
	writingSpot = x;
	//Iterates through each character in a string and calls draw_letter() for each.
	while (text[i] != '\0') {
		draw_letter(writingSpot, y, text[i], c);
		i++;
	}
}

void draw_letter(int x, int y, char c, color_t color) {
	int i;
	int row = y;
	int col = x;

	//Locates the 16 bytes of the specified ASCII character in iso_font arr and iterates through each.
	for (i = c * 16; i < c * 16 + 16; i++) {

		unsigned char num = iso_font[i];
		int bitPos = 0;
		int numOfBits = 0;
		//Iterates though each bit of each Byte and draws a pixel if the bit is 1.
		for (numOfBits; numOfBits < 8; numOfBits++) {
			if (num >> numOfBits & 0x01) {
				draw_pixel(col + bitPos, row, color);
			}
			else {
			}
			bitPos++;
		}
		row++;
	}
	writingSpot += 9;
}

