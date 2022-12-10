/*
* @Author: Philip Jochems
* @Class: CSC452
* @Description: Simple graphics library written in C. Has the ability
* to write text, create a square and read in input. 
*/

//Imports
#include <sys/mman.h>
#include "graphics.h"
#include <stddef.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <termios.h>
#include <sys/select.h>
#include <stdio.h>
#include "iso_font.h"
#include <fcntl.h>

//Global values
typedef unsigned short color_t;
char* file_buffer;
struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;
struct termios term;
int file;


/*
* Initializes the graphics
*/
void init_graphics(){

	file = open("/dev/fb0",O_RDWR);
	int var_getter = ioctl(file,FBIOGET_VSCREENINFO,&var);
	int fix_getter = ioctl(file,FBIOGET_FSCREENINFO,&fix);
	file_buffer=mmap(0, var.yres_virtual*fix.line_length, PROT_WRITE | PROT_READ, MAP_SHARED, file, 0); 
	ioctl(0,TCGETS, &term);
	term.c_lflag &= ~ECHO;
	term.c_lflag &= ~ICANON;
	ioctl(0,TCSETS,&term);
	clear_screen();
}

/*
* Exits the graphics
*/
void exit_graphics(){
	ioctl(0,TCGETS,&term);
	term.c_lflag |= ECHO;
	ioctl(0,TCSETS,&term);
	munmap(NULL,0);
	close(file);
}

/*
* Clears the scren of all pixels
*/
void clear_screen(){
	write(1,"\033[2J",4);
}

/*
* Retrieves and returns the key pressed, if a key was pressed
*/
char getkey(){
	struct timeval my_time;
	my_time.tv_sec=0;
	my_time.tv_usec=0;
	char buffer[255];
	fd_set temp; 
	FD_ZERO(&temp);
	FD_SET(0, &temp);
	int value = select(1, &temp,0,0,&my_time);
	unsigned char char_input;
	
	if(value==1){
		int read_char = read(0,&char_input,sizeof(char_input));
		if(read_char==0){
			return read_char;
		}else{
			return char_input;
		}
	}
	return 0;
	
}

/*
* Sleeps the system for ms amount of milliseconds
* @param ms - Time in milliseconds to sleep for
*/
void sleep_ms(long ms){
	struct timespec curr,curr2;
	curr.tv_sec=0;
	curr.tv_nsec=ms*1000000;
	nanosleep(&curr,curr2);
}


/*
* Draws a pixel at position (x,y) with color c
* @param x - x position
* @param y - y position
* @param color - color of pixel
*/
void draw_pixel(int x, int y, color_t color){
	int offset =(x+y*(var.xres))*2;
	if(y<var.yres){
		file_buffer[offset]=color&((1<<5)-1);
		file_buffer[offset+1]=color&(((1<<6)-1)<<5);
		int red_mask=((1<<5)-1)<<11; // Warning otherwise
		file_buffer[offset+2]=color&red_mask;
	}
}

/*
* Draws a rectangle starting at (x1,y1) with width "width" and height "height"
* and color c
* @param x1 - x position
* @param y1 - y position
* @param width - width
* @param height - height
* @param color - color of square
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){
	int i=0;
	int j=0;
	for(i=0;i<width;i++){
		draw_pixel(x1+i,y1,c);
		draw_pixel(x1+i,y1+height-1,c);
	}
	for(j=0;j<height;j++){
		draw_pixel(x1,y1+j,c);
		draw_pixel(x1+width-1,y1+j,c);
	
	}
}


/*
* Draws a text at (x,y) with text text and of color c
* @param x - x position
* @param y - y position
* @param text - text to be drawn
* @param color - color of text
*/
void draw_text(int x, int y, const char *text, color_t c){
	int i=0;
	int size =0;
	const char *text_pointer;
	for(text_pointer=text;*text_pointer!='\0';text_pointer++){
		size++;
	}
	for(i=0;i<size;i++){
		int j=0;
		for(j=0;j<16;j++){
			int row=iso_font[text[i]*16+j];
			int z =0;
			for(z=0; z<8;z++){
				unsigned mask;
				mask =1;
				int val = (row >> 7-z )& mask;
				if(val!=0){
					draw_pixel(x*(i+1)+(7-z),y+j,c);
				}
			}
		}
	}
}
