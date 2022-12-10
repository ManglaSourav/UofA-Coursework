/**
Author: Hassan Alnamer
This file builds a graphics library that will operate on 
/dev/fb0/
It will be a library used by  driver.c

Functions built in the file:
	void init_graphics() ->open, ioctl, mmap
	void exit_graphics() -> ioctl
	void clear_screen()-> write
	char getkey()-> select, read
	void sleep_ms(long ms) -> nanosleep
	void draw_pixel(int x, int y, color_t color) 
	void draw_rect(int x1, int y1, int width, int width, 
	color_t c)
	void draw_text(int x, int y, const char* text, 
	color_t c)
	
	
	
*/
#include "iso_font.h"
#include "graphics.h"
/*
This function will open the graphics
Graphics device is at 
/dev/fb0
**/
//for open sys call
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
//to be able to write, could be removed
#include <unistd.h>
//for mmap
#include <sys/mman.h>
//to use ioctl
#include <sys/ioctl.h>
//to use fb structures
#include <linux/fb.h>
//to be able to use terminal settings
#include <termios.h>
//to use select
#include <sys/time.h>
#include <sys/types.h>
static int fd;
color_t *mapped;
static int size;
static int s_height;
static int s_width;

/**
Helper function for printing to stderr
*/
void print_err(char msg[]){
	write(STDERR_FILENO, &msg, sizeof(msg)-1);
}

void init_graphics(){
	struct fb_var_screeninfo virtual_resolution;
	struct fb_fix_screeninfo for_bit_depth;
	
	//open device
	char* filename = "/dev/fb0";
	fd = open(filename, O_RDWR);
	if(fd < 0){

		return;
	}
	
	if(ioctl(fd, FBIOGET_FSCREENINFO,  &for_bit_depth) == -1){
		return;
	}
	if(ioctl(fd, FBIOGET_VSCREENINFO, &virtual_resolution) == -1){

		return;
	}
	
	//map to program for better handling
	
	s_height = virtual_resolution.yres_virtual;
	s_width = for_bit_depth.line_length/2;
	size =  2 * s_height * s_width;
	mapped =(color_t*) mmap(NULL, size, PROT_READ | PROT_WRITE, 
	MAP_SHARED, 
	fd ,0);
	if(mapped == MAP_FAILED){
		return;
	}
	//there should be write() down here
	//....
	struct termios terminal_settings;
	if(ioctl(STDIN_FILENO, TCGETS, &terminal_settings) == -1){
		return;
	}
	terminal_settings.c_lflag &= ~(ICANON | ECHO);
	if(ioctl(STDIN_FILENO, TCSETS, &terminal_settings) == -1){
		return;
	}

	is_init=1;
}

void exit_graphics(){
	//notify program
	if (is_init == 0) return;
	is_init=0;
	//should unmap the memory
	if(munmap(mapped, size) == -1){
		return;
	}
	//should close the fd
	if(close (fd) == -1){
		return;
	}
	//shoudl clean up terminal settings termios
	struct termios terminal_settings;
	if (ioctl(STDIN_FILENO, TCGETS, &terminal_settings) == -1){
		return;
	}
	//could be an error
	terminal_settings.c_lflag |= (ICANON | ECHO);
	if(ioctl(STDIN_FILENO, TCSETS, &terminal_settings) == -1){
		return;
	}
}
/**
This function is supposedd to write
\033[2J 
to either dev/bf0 or stdout I am not sure yet
*/
void clear_screen(){
	if(is_init == 0) return;
	char *ansi = "\033[2J";
	write(STDOUT_FILENO, ansi, sizeof(&ansi));
	return;
}
/**
This function uses select() to listen for user input
We want to listen to stdin
and upon event change something in /dev/bf0
Thus we will not set a time interval
might be subject to change regarding timeval
& the output file
*/

char getkey(){
	if(is_init==0) return;
	//input that we are listining to
	char input = '\0';
	//set fd_set values
	fd_set listen;
	//not sure if I should use it
	fd_set on_action;
	//per feedback timeout should be 0
	//this way select will not interrupt
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO(&listen);
	FD_SET(STDIN_FILENO, &listen);
	FD_SET(fd, &on_action);
	int retval = select(1, &listen, NULL, NULL, &timeout);
	if(retval == -1){
		char err[] = "select returned -1\n";
		
		//not sure about the returns
		return input;
	}
	if(retval){
		read(STDIN_FILENO, &input, sizeof(input));
		char msg1[] = "select caught:\n";
		char msg[sizeof(msg1)+1] = "select caught: \n";
		msg[sizeof(msg1)-1] = input;
		
	}
	return input;

}
/**
stops the thread fo rsome time
*/
void sleep_ms(long ms){
	if(ms < 0){
		return;
	}
	long million =  1000000;
	struct timespec sleepy_time;
	sleepy_time.tv_sec=0;
	sleepy_time.tv_nsec = ms * million;
	//hopefull there are no errs
	//since it was declared in doc
	//but, just in case
	int retval = nanosleep(&sleepy_time, NULL);
	if(retval == -1){
		char msg[]="nanosleep is not happy";
		
	}
}
void draw_pixel(int x, int y, color_t color) {
	//check coordinates

	if(x < 0 || y < 0 || x >= s_width || y >= s_height){
		return;
	}
	//determine the offset
	int offset = x + (y * s_width);
	color_t *pixel = mapped + offset;
	*pixel = color;
}
/**
This function will use draw pixel and a while loop
to draw a rectangle.
*/

void draw_rect(int x1, int y1, int width, int height, 
	color_t c){
	//check that point is inside screen
	if(x1 < 0 || y1 < 0 || y1 > s_height ||
	x1 > s_width){
		
		return;
	}
	if(x1+width > s_width || x1+width < 0 || 
	y1+height > s_height ||
	y1+height < 0){
		
		return;
	}
	//starting point is (x1, y1)
	//then we have to increment them
	int counter = 0;
	short draw = 1;
	//draw parallel lines in the same loop
	while(counter < width){
		draw_pixel(x1++, y1, c);
		counter++;
	}
	counter = 0;
	while(counter < width){
		draw_pixel(x1--, y1+height, c);
		counter++;
	}
	counter = 0;
	while(counter < width){
		draw_pixel(x1, y1++, c);
		counter++;
	}
	counter = 0;
	while(counter < width){
		draw_pixel(x1+width, y1--, c);
		counter++;
	}
	counter = 0;
}
/**
This function draws a letter using iso font
*/
void helper_text(int x, int y, const char l, color_t c){
	int row = 0;
	while(row<16){
		int bit = 0;
		int curr_l = iso_font[l*16 + row];
		while(bit < 8){
			int is_draw = curr_l & 0x01;
			curr_l >>= 1;
			if(is_draw){
				draw_pixel(x+bit, y+row, c);
			}
			bit++;
		}
		row++;
	}

}
void draw_text(int x, int y, const char* text, 
	color_t c){
	int i = 0;
	while(text[i] != '\0'){
		helper_text(x+ (i*8), y, text[i], c);
		i++;
	}
}
