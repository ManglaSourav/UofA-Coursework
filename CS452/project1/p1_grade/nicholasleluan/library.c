/*
Author: Nicholas Leluan
CSC 452; Dr. Misurda
Spring 2022

This file contains the begin logics of a graphics library in C only 
using system calls. 
At the moment, this library has draw text and draw rectangle 
functionality. 
The usage of these functions is planned to be ported to any device that 
has a framebuffer. At the moment, it has only been tesed with QEMU 
virtual machine. 
*/


#include "iso_font.h" // for the fonts
#include "graphics.h" // header file 
#include <time.h> //for nanosleep()
#include <sys/select.h> //for select()
#include <unistd.h>
#include <fcntl.h> //for file macros
#include <sys/mman.h> //for mmap()
#include <sys/ioctl.h> // for icotl
#include <termios.h> // for termios
#include <stdio.h> // for debug only
#include <linux/fb.h> // for screen info

/*
Initializes the graphics library:
1) Will open a graphics device; will need to read a file from /dev/fb0 
2) Will write to the screen by:
	a)asking the OS to map a file into our address space (which will 
	be an array we can load/store in) that will represent a screen 
	where each index is a pixel on said screen.
3) Use configs from the specified screen (the one used by QEMU) with the 
following specifications:
	- Width(x)  : 640 px
	- Height(y) : 460 px
	- Color     : 16-bit congfig (5-6-5) RGB(RRRRR-GGGGGG-BBBBB) 
	** might need to use ioctl() here to get these values without 
	hard-coding them.
4) Use ioctrl() to disable keypress echo(display as we are typing) 
	- need to disable canonical mode by setting ICANON bit and 
	disabling ECHO by forcing these bits to be 0(zero)
		- CALLING exit_graphics() will reset these values!
*/
int SCREEN_W;
int SCREEN_H;
int BIT_DEPTH;
char * fmap;
int fptr;
int size;
struct termios configsOn,configsOff; //on is for when prohgram is 
// running; off is for when program is exiting

void init_graphics(){
	// opening the framebuffer
	fptr = open("/dev/fb0",O_RDWR);// READ & WRITE
	if(fptr >= 0){
		struct fb_var_screeninfo vScreen;
		struct fb_fix_screeninfo fScreen;
		ioctl(fptr,FBIOGET_VSCREENINFO, &vScreen);
		ioctl(fptr,FBIOGET_FSCREENINFO, &fScreen);
		SCREEN_W = fScreen.line_length;
		SCREEN_H = vScreen.yres_virtual;
		BIT_DEPTH = vScreen.bits_per_pixel;
		size = vScreen.yres_virtual * fScreen.line_length; 
		fmap = mmap(0, size, 
			PROT_READ|PROT_WRITE,MAP_SHARED,fptr,0);
		// turing off ECHO and set ICANON bit
		ioctl(0,TCGETS,&configsOff);
		configsOn = configsOff;
		configsOn.c_lflag &= ~(ECHO|ICANON);
		ioctl(0,TCSETS,&configsOn);
	}
	clear_screen();
}

/*
Function that take in 3 int values, and converts them to a 16-bit short 
value representing a 5-6-5 RGB coding.
Function takes into consideration if passed in values are above the 
alotted amunt of bits for a color. If a passed in value is larger, th 
value is set to the max bit count for that color(31 for blue and red, 63 
for green).
*/
color_t get_RGB_565(int r, int g, int b){
	unsigned short r_mask = 0x1F;//fill 5 bits
	unsigned short g_mask = 0x3F;//fill 6 bits
	unsigned short b_mask = 0x1F;//fill 5 bits
	if(r > 31) r = 31;
	if(g > 63) g = 63;
	if(b > 31) b = 31;
	color_t ret = ((r & r_mask) << 11) | ((g & g_mask) << 5) | (b &  
	b_mask);
	return ret;
}


/*
Cleanup anything that needs to be reset back to standard after the 
program has terminated (i.e. called this function)
	-Definitely needs to reenable key press echoing and buffering 
	(see init_graphics() comment #4).
*/
void exit_graphics(){
	ioctl(0,TCSETS,&configsOff);
	munmap(fmap,size);
	close(fptr);
}
/*
Uses ANSI escape code - which is a sequence of characters that are not 
meant to be displaye as text, but rather interpreted as commands to the 
terminal. 
*/
void clear_screen(){
	write(STDOUT_FILENO,"\033[2J",4);
}

/*
Will make use of the Linux system call select() which is a non-bocking 
system call that signifies if there has been a keypress (think of this 
like an interupt).If there was a key-press, read it!
*/
char getkey(){
	// select(int nfds, fd_set *readfds, fd_set write_fds, fd_set 
	//error fds, struct timeval timeout)
	struct timeval t;
	int keyPressed;
	t.tv_sec = 0;
	t.tv_usec = 0;
	fd_set fileDescriptorSets; // a bit array repr. file sets for 
	// select()
	FD_ZERO(&fileDescriptorSets); // initializes the file set to be 
	//empty
	//STDIN_FILENO=> file descriptor; should be 0
	FD_SET(STDIN_FILENO,&fileDescriptorSets); // 0 is to read from 
	//stdin;lookup file descriptors in C.
	// should we wait here?
	// need to get the input
	keyPressed = select(STDIN_FILENO+1, 
		&fileDescriptorSets,NULL,NULL,&t);//is this 
	// 1 to read whats in stdin? Read from /dev/tty (ref 
	//GeeksForGeeks on file descriptors in C
	if(keyPressed){
		char buff[1];
		read(0,buff,1);
		int x = buff[0];
		return x;
		//this is buggy, will return more than just one char 
		//including \n
	}
	return 0;

}

/*
Function that allows our program to sleep in between frames that are 
actively being drawn. This will use nanosleep() system call; simply 
sleep for specified number of milliseconds then multiply this number by 
1,000,000. 
*/
void sleep_ms(long ms){
	// second param of nanosleep() can be NULL ; erase when done
	long mill = 1000000;
	struct timespec sleepTime;
	sleepTime.tv_sec  = ms / 1000;
	sleepTime.tv_nsec = (ms%1000) * mill;
	nanosleep(&sleepTime,NULL);
}

/*
MAIN DRAWING CODE
Set the pixel using the passed in x and y coordinates to the specified 
color (16-bit struct value)
*/
void draw_pixel(int x, int y, color_t color){
	int i = (x*(BIT_DEPTH)/8) + y*SCREEN_W; 
	int r = (color & 0xF800); // RED MASK
	int g =  (color & 0x07E0);// GREEN MASK
	int b = (color & 0x001f); // BLUE MASK
	// combine masks to form 1 16 bit short
	*((unsigned short int*)(fmap + i)) = r | g | b;
}

/*
Uses praw_pixel() to draw a rectangle to screen with the following 
corners:
(x1,y1), (x1+width, y1), (x1+width, y1+width), (x1, y1+width)
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){
	int row;
	int col;
	if(y1+height > SCREEN_H){
		y1 = SCREEN_H-height;
	}
	if(y1 < 0){
		y1 = 0;
	}
	// This currently draws the entire rectangle filled in
	// some a**holes are saying this needs to be the STROKE
	// of the shape, but I dont believe them 
	for(row = y1; row < y1+height; row++){
		for(col = x1; col < x1+width; col++){
			draw_pixel(col,row,c);
		}
	}
}

/*
Draw the passed in text using the x and y values as its location 
(coordinates that start in upper-left corner of FIRST letter). 
This function makes use of iso_font.h which will be used to access a 
font encoded into an array that represents a 8x16 pixel map where each 
row represents 1-byte (8 bit) integer. 
*/
void draw_text(int x, int y, const char *text, color_t c){
	int pos = 0;// keeps track of the position the letter is in
	int crtr = 0;
	while(text[pos] != 0){
		int x_pos = 8*pos + x;
		draw_letter(x_pos,y,text[pos],c);
		pos++;
	}

}

/*
Helper function to draw an indiviusal letter on the screen using the 
passed in coordinates (determined and vetted in another function).
This function uses iso_font which is an array that contains all the bits 
for an associated text font. To get the correct mapping for a particular 
letter, use the followig formla:
((int) LETTER*16+0) - ((int)LETTER*16+15);
*/
void draw_letter(int x, int y, int letter, color_t c){
	int begin = (letter*16 + 0);//start in iso_font array
	int end   = (letter*16 + 15);//end in iso_font array
	int row = 0;
	while(begin <= end){
	// logic here to  get the data from iso_font
	// iso_font is a 256*16 size array
		unsigned char row_data = iso_font[begin];
		int bit = 0;
		// 8 bits per row
		while(bit < 8){
			if(row_data & 0x01){
				int draw_x = bit+x;
				int draw_y = row+y;
				// logic to not cause draw outside of 
				// array
				if(draw_y >= 0 && draw_y < SCREEN_H){
					draw_pixel(draw_x,draw_y,c);
				}
			}
			bit++;
			row_data = row_data >> 1;
		}
		begin++;
		row++;
	}
}
