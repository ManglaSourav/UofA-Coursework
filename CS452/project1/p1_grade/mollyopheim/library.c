/* 
 * Author: Molly Opheim
 * Class: CSc 452
 * Project 1
 * In library.c, the library calls init_graphics, exit_graphics, 
 clear_screen, getkey, sleep_ms, draw_pixel, draw_rect, and draw_text 
 are implented. These library calls provide library functions for some 
 graphics. 
 */
#include "iso_font.h"
#include "graphics.h"

/**
* Initialzies the graphics library.
* This is first done by opening the file /dev/fb0 that represents the 
* frame buffer. 
* Then mmap is used to memory map the contents from that file so 
* that the rest of the graphics can be drawn.
*/
void init_graphics()
{
	// /dev/fb0 is the file that represents the contents of the
	// frame buffer
	fileDes = 0;
	fileDes = open("/dev/fb0", O_RDWR);
	if(fileDes == -1) {
		perror("not able to read /dev/fb0");
	}
	// these calls get information about the screen
	ioctl(fileDes, FBIOGET_FSCREENINFO, &fscreen);
	ioctl(fileDes, FBIOGET_VSCREENINFO, &vscreen);
	mmapSize = vscreen.yres_virtual *
	fscreen.line_length;

	// maps the contents from /dev/fb0 to memory
	mPtr = mmap(NULL, mmapSize, PROT_READ | PROT_WRITE, 
	MAP_SHARED,
	fileDes, 0);

	if(mPtr == MAP_FAILED)
		printf("map failed");
	
	// disabling key press
	tcgetattr(0, &cur_t);
	new_t = cur_t;
	new_t.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &new_t);


}

/**
* This draws a pixel to the screen. 
*
* int x - the x coordinate of where the pixel should be drawn
* int y - the y coordinate of where the pixel should be drawn
* color_t color - the color of the pixel to be drawn
*/
void draw_pixel(int x, int y, color_t color) {
	int spot = (x) * (vscreen.bits_per_pixel/8) +
	(y) * fscreen.line_length;
	
	*((color_t*) (mPtr+spot)) = color;
}

/**
* This draws a rectangle to a screen utilizing the draw_pixel method
*
* int x1 - the starting x coordinate of the rectangle
* int y1 - the starting y coordinate of the rectangle
* int width - the width of the rectangle
* int height - the height of the rectanlge
* color_t c - the color of the rectangle to be drawn
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int i, j;
	int screenWidth = fscreen.line_length/2;
	int screenHeight = vscreen.yres_virtual;
	if(x1 + width > screenWidth || y1 + height > 
	screenHeight) 
	{
		printf("could not print rectangle, not in bounds");
		exit_graphics();
	} else {
		for(i = x1; i <= x1 + width; i++) {
			for(j = y1; j <= y1 + height; j++) {
				draw_pixel(i, j, c);
			}
		}
	}
}

/**
* Draws text to the screen
*
* int x - the starting x coordinate of the text
* int y - the starting y coordinate of the text
* const char *text - the pointer to the text to draw
* color_t c - the color of the text to draw the screen
*/
void draw_text(int x, int y, const char *text, color_t c) {
	char *s;
	size_t i = 0;
	// the dimensions of x and y for each char
	int xdim = 8;
	int ydim = 16;
	int screenWidth = fscreen.line_length/2;
	int screenHeight = vscreen.yres_virtual;
	while(text[i] != '\0') {
		if(x + xdim > screenWidth) {
			y += ydim;
			if(y + ydim > screenHeight) {
				printf("error drawing text");
			} else {
				x = 0;
				draw_char(x, y, text[i], c);
			}
		} else {
			draw_char(x, y, text[i], c);
		}
		x += xdim;
		i++;
	}
}

/**
* Draws a single character
*
* int x - the starting x coordinate of the character
* int y - the starting y coordiante of the character
* char letter - the letter to draw
* color_t c - the color to draw
*/
void draw_char(int x, int y, char letter, color_t c) {
	int i;
	for(i = 0; i < 16; i++) {
		int row = iso_font[letter*16+i];
		int count = 0;
		while(row) {
			if(row & 1)
				draw_pixel(x + count, y + i, c);
		row >>=1;
		count += 1;
		}
	}
}

/**
* Sleeps the graphics being drawn
*
* long ms - the number of milliseconds to sleep for
*/
void sleep_ms(long ms) {
	struct timespec req = {
		(int)(ms/1000),
		(ms)* 1000000
	};
	//struct timespec rem;
	nanosleep(&req, NULL);
}

/**
* Reads user input, but does not block because of the use of select()
*
* Returns the char typed as key press input
*/
char getkey() {
	fd_set fset;
	struct timeval tval;
	tval.tv_sec = 0;
	tval.tv_usec = 0; 
	int sel = select(1, &fset, NULL, NULL, &tval);
	//int res = read(0, stdin, 128);
	//printf("%d", res);
	char ch;
	while(read(STDIN_FILENO, &ch, 1) > 0) {
		return ch;
	}
}

/**
* Clears the terminal 
*/
void clear_screen() {
	printf("\033[2J");
}

/**
* Cleans up the graphics so that the program can terminate.
* In this method we close the unmap the memory mapped and reenable
* the key press
*/
void exit_graphics() {
	close(fileDes);
	munmap(mPtr, mmapSize);
	tcsetattr(0, TCSANOW, &cur_t);
}

