 /* File: library.c
 *
 * Author: Zachary Taylor
 * NetID: ztaylor
 * Class: CSC 452
 * Assignment: HW1
 *
 * Provides definitions for all functions from the spec
 *   Gives us the ability to use the linux frame buffer to draw specific
 *	 pixels, text, and rectangles.
 */

#include <sys/mman.h>
#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "iso_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <assert.h>
#include <errno.h>
#include <linux/fb.h>
typedef int16_t color_t;
	int fBuff;
	struct fb_var_screeninfo fb_var;
	struct fb_fix_screeninfo fb_fix;
	size_t vert;
	size_t hor;
	size_t total;
	color_t *map;
	struct termios first,second;
	fd_set fileDes;
	
	
/* Purpose: clears the screen of text or images
*/
void clear_screen(){
	write(1,"\033[2J", 4);
}

/* Purpose: initiates the frame buffer and sets the size so specific pixels
*	can be manipulated.  Sets terminal settings so that key presses are not
*	shown.
*/
void init_graphics(){
	fBuff = open("/dev/fb0", O_RDWR);
	if(!ioctl(fBuff, FBIOGET_VSCREENINFO, &fb_var)){
		vert = fb_var.yres_virtual;
	}
	if(!ioctl(fBuff, FBIOGET_FSCREENINFO, &fb_fix)){
		hor = fb_fix.line_length / 2;
	}
	total = vert*hor*2;
	off_t offset = 0;
	map = mmap(NULL, total, PROT_READ|PROT_WRITE, MAP_SHARED, fBuff, offset);
	ioctl(0, TCGETS, &first);
	second = first;
	second.c_lflag &= ~(ICANON | ECHO);
	ioctl(0, TCSETS, &second);
	clear_screen();
}
/*Purpose: resets terminal settings to their original, unmaps the memory map
*	and closes the open frame buffer
*/
void exit_graphics(){
	ioctl(0, TCSETS, &first);
	clear_screen();
	
	munmap(map, total);
	close(fBuff);
}

/*Purpose: lets the program read key presses without blocking.
  Return: the character of the key pressed
*/
char getkey(){
	FD_ZERO(&fileDes);
	FD_SET(0, &fileDes);
	struct timeval timer;
	timer.tv_sec = 0;
	timer.tv_usec = 0;
	int retval = select(STDIN_FILENO + 1, &fileDes, NULL, NULL, &timer);
	if(retval == 1){
		char key;
		read(0, &key, 1);
		return key;
	}else{
		return 0;
	}
}
/*Purpose: uses the given long to sleep the process and allow the user to see the data that has been drawn.
* Param: ms is the time in miliseconds to sleep.
*/
void sleep_ms(long ms){
	struct timespec timer;
	timer.tv_sec = ms/1000;
	timer.tv_nsec = (ms%1000)*1000000;
	nanosleep(&timer, NULL);
}
/*Purpose: Draws a specific color pixel at the given x and y coordinates using
*	the created mmap
* Param: x is the int for x coordinate
*	y is the int for the y coordinate
*	color is a 16bit color value
*/
void draw_pixel(int x, int y, color_t color){
	if((y > 0) && (y < 480)){
		map[fb_var.xres_virtual * y + x] = color;
	}
}
/*Purpose: Draws a specific color rectangle of the given width and height
*	starting at the given x and y coordinates
* Param: x1 is the int for x coordinate
*	y1 is the int for the y coordinate
*	c is a 16bit color value
*	width is the width of the rectangle to be drawn
*	height is the height of the rectangle to be drawn
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){
	int i;
	int j;
	for(i = 0; i < width; i++){
		draw_pixel(x1+i,y1, c);
		draw_pixel(x1+i,y1+height, c);
	}
	for(j = 0; j < height; j++){
		draw_pixel(x1,y1+j, c);
		draw_pixel(x1+width,y1+j, c);
	}
}
/*Purpose: Draws a specific color letter starting at the given x and y 
*	coordinates.  Uses iso_font to find which pixels should be drawn
* Param: x is the int for x coordinate
*	y is the int for the y coordinate
*	c is a 16bit color value
*	letter is the ASCII value of the letter to be drawn
*/
void draw_letter(int x, int y, int letter, color_t c){
	int i;
	int j;
	int pow;
	for(i = 0; i < 16; i++){
		int lineVal = iso_font[letter*16+i];
		for(j = 7; j > 0; j--){
			int mask = 1;
			for(pow = 0; pow < j; pow++){
				mask *= 2;
			}
			int pixel = lineVal & mask;
			if(pixel>>j == 1){
				draw_pixel(x+j,y+i,c);
			}
		}
			
	}
}
/*Purpose: Draws a specific color string of text
*	starting at the given x and y coordinates
* Param: x is the int for x coordinate
*	y is the int for the y coordinate
*	c is a 16bit color value
*	text is the pointer to the string
*/
void draw_text(int x, int y, const char *text, color_t c){
	const char *letter = text;
	while(*letter != '\0'){
		draw_letter(x, y, (int)*letter, c);
		letter++;
		x += 8;
	}
}
