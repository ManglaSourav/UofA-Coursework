/*
Author: Emilio Santa Cruz
Class: 452 @ Spring 2022
Professor: Dr. Misurda
Assignment: Project 1
Description: Serves as library of calls to draw on the terminal of a 
linux vm. Provides a blank canvas and can draw pixel, rectangles or text 
from Apple's provided front "iso_font"
*/ 

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/fb.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include "iso_font.h"

typedef unsigned short color_t;

// globals
int fd;
unsigned short* frameBuffer;
struct termios oldIOBuffer;
struct fb_var_screeninfo vInfo;
struct fb_fix_screeninfo fInfo;

/*
Clears the screen to setup for drawing.
Parameters: None
Pre-Conditions: init_graphics() is called or an individual request is 
made to clear the screen.
Post-Conditions: The screen is blank
Return: None
*/
void clear_screen(){
	write(fd, "\033[2J", 4);
}

/*
Initializes the above global variables, removes ICANON and ECHO, and 
clears the screen.
Parameters: None
Pre-Condition: A initialization is requested
Post-Conditions: The screen is cleared, ICANON and ECHO are removed and 
the above variables are set.
Return: None
*/
void init_graphics(){
	clear_screen();
	
	fd = open("/dev/fb0", O_RDWR);
	
	if(ioctl(fd, FBIOGET_VSCREENINFO, &vInfo) == -1){
		printf("%s", "fb_var_screeninfo");
	}
	if(ioctl(fd, FBIOGET_FSCREENINFO, &fInfo) == -1){
		printf("%s", "fb_fix_screeninfo");
	}
	long size = vInfo.yres_virtual * fInfo.line_length;
	frameBuffer = (unsigned short*)mmap(NULL, size, PROT_READ | 
	PROT_WRITE, 
	MAP_SHARED, fd, 0);

	struct termios newIOBuffer;
	if(ioctl(STDIN_FILENO, TCGETS, &oldIOBuffer) == -1){
		printf("%s\n", "TCGETS");
	}
	newIOBuffer = oldIOBuffer;
	newIOBuffer.c_lflag &= ~(ICANON | ECHO);
	
	if(ioctl(STDIN_FILENO, TCSETS, &newIOBuffer) == -1){
		printf("%s\n", "TCSETS REMOVE");
	}
}

/*
Restores ICANON and ECHO to the terminal
Parameters: None
Pre-Conditions: Drawing is done
Post-Conditions: ICANON and ECHO are restored
Return: None
*/
void exit_graphics(){
	if(ioctl(STDIN_FILENO, TCSETS, &oldIOBuffer) == -1){
		printf("%s\n", "FAILED BUFFER RESTORE");
	}
	munmap((void*) frameBuffer, vInfo.yres_virtual * 
	fInfo.line_length);
	close(fd);
}

/*
Gets any key downs entered to terminal and returns them.
Parameters: None
Pre-Condition: The program is waiting for an input
Post-Condition: Either 0 or an inputed ch is returned.
Return: 0 or a char
*/
char getkey(){
	fd_set input;
	struct timeval tv;
	int retval;
	
	FD_ZERO(&input);
	FD_SET(0, &input);
	
	tv.tv_sec = 0; tv.tv_usec = 0;
	
	retval = select(1, &input, NULL, NULL, &tv);

	if(retval == 1){
		char key;
		read(STDIN_FILENO, &key, 1);
		return key;
	}
	
	return 0;
}

/*
Sleeps the program for ms time
Parameters: ms is the long integer representing the time to wait in 
milliseconds.
Pre-Conditions: A call to sleep is made
Post-Conditions: A certain amount of ms have passed equal to ms.
Return: None
*/
void sleep_ms(long ms){
	struct timespec sleepTime = {0, ms*1000000};
	nanosleep(&sleepTime, NULL);
}

/*
Draws a single pixel at x, y with the color color.
Parameters: x is the x coordinate of the pixel, y is the y coordinate of 
the pixel, c is the color to write in
Pre-Conditions: A pixel is desired to be drawn either individually or 
from draw_rect or draw_text
Post-Conditions: A pixel is drawn in c color and is at x,y
Return: None
*/
void draw_pixel(int x, int y, color_t c){
	int currX = x % vInfo.xres;
	int currY = y % vInfo.yres;
	if(currY < 0){
		currY = currY + vInfo.yres;
	}
	if(currX < 0){
		currX = currX + vInfo.xres;
	}
	int loc = currY * vInfo.xres + currX;

	frameBuffer[loc] = c;
}

/*
Draws a rectangle at x1,y1 with described width and height with color c
Parameters: x1 is the left starting point of the rectangle, y1 is the 
top starting point of the rectangle, width is the width of the 
rectangle, height is the height of the rectangle, c is the color to 
write in.
Pre-Conditions: A rectangle is requested to be drawn.
Post-Conditions: A rectangle is drawn.
Return: None
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){
	int x = x1; int y = y1;
	int xWidth = x + width;
	int yHeight = y + height;
	while(y <= yHeight){
		while(x <= xWidth){
			draw_pixel(x, y, c);
			x++;
		}
		y++;
		x = x1;
	}
}

/*
Draws the char ch at x,y with the color c.
Parameters: x is the x coordinate to start at, y is the y coordinate to 
start at, ch is the char to write, c is the color to write in.
Pre-Condition: A string is requested to be written.
Post-Condition: ch is written on screen.
Return: None
*/
void draw_char(int x, int y, const char ch, color_t c){
	int i = 0;
	int j = 0;
	unsigned char currChar;
	int charIndex = (int) ch;
	while(i < 16){
		currChar = iso_font[charIndex * 16 + i];
		while(j < 8){
			if(currChar & 0x01){
				draw_pixel(x + j, y + i, c);
			}
			currChar = currChar >> 1;
			j++;
		}
		j = 0;
		i++;
	}
}

/*
Draws the described text string at x,y with the color c
Parameters: x is the x cordinate to start at, y is the y coordinate to 
start at, text is the string to write, c is the color to write with
Pre-Condition: A line of text is to be written.
Post-Conditions: text is written on screen.
Return: None
*/
void draw_text(int x, int y, const char *text, color_t c){
	int i = 0;
	while(text[i] != '\0'){
		draw_char(x + i * 24, y, text[i], c);
		i++;
	}
}
