/*
 Author: Christian Trejo
 Course: CSC452
 Assignment: Project 1: Graphics Library
 Due: Sunday, February 6, 2022 by 11:59pm
 Purpose: Provides various library functions using only the Linux system calls
          without the use of any C standard library functions.
*/

#include "library.h"	//Header file for below functions
#include "iso_font.h"	//Font encoded into an array
#include <time.h>	//nanosleep()
#include <fcntl.h> 	//open()
#include <sys/mman.h>   //mmap()
#include <sys/ioctl.h>  //ioctl()
#include <linux/fb.h>	//ioctl() structs
#include <termios.h>	//terminal setting struct
#include <unistd.h>	//close(), read()
#include <sys/select.h>	//select()


//Globals:
char *file_addr;	//Pointer to mapped area
int file_desc; 		//File descriptor
int array_length;	//Memory array length
int screen_height;	//y-resolution of screen
int screen_width;	//x-resolution of screen
int line_len;		//Length of line in bytes
int bits_per_pixel;	//Bits per pixel


/*
Purpose: This function will encode a color_t from three RGB values using
	 bit shifting and masking to make a single 16-bit number.
Syscalls: None
Parameters:
	red - 5-bit number for red intensity; int type
	green - 6-bit number for green intensity; int type
	blue - 5-bit number for blue intensity; int type
Return: color - 16-bit color; color_t type
*/
color_t getRGB(color_t red, color_t green, color_t blue){

	color_t ret_col = (red << 11) & 0xf800;	  //first 5 bits are red
	ret_col = (green << 5) | ret_col;	  //middle 6 bits green
	ret_col = blue | ret_col;		  //last 5 bits blue
	return ret_col;
}



/*
Purpose: This function will initialize the graphics library.
Syscalls: open(), ioctl(), mmap()
Parameters: None
Returns: None
*/
void init_graphics(void){

	//Open file and save file descriptor
	file_desc = open("/dev/fb0", O_RDWR);

	//Check if open() errored
	if(file_desc == -1){
		write(2, "Error when opening file.\n", 25);
	}


	//Get structs for array length calculation
	//Get struct holding virtual resolution
	struct fb_var_screeninfo v_screen;
	int ret_val1 = ioctl(file_desc, FBIOGET_VSCREENINFO, &v_screen);
	if(ret_val1 == -1){	//Check if errored getting virt. res info
		write(2, "Error when getting screen res info.\n", 36);
	}

	//Get struct holding bit depth
	struct fb_fix_screeninfo f_screen;
	int ret_val2 = ioctl(file_desc, FBIOGET_FSCREENINFO, &f_screen);
	if(ret_val2 == -1){	//Check if errored getting bit depth
		write(2, "Error when getting bit depth struct.\n", 37);
	}

	//Save screen values in globals for later use
	screen_height = v_screen.yres_virtual;	  //y-resolution
	screen_width = v_screen.xres_virtual;	  //x-resolution
	line_len = f_screen.line_length;	  //length of line (bytes)
	bits_per_pixel = v_screen.bits_per_pixel; //bits per pixel

	//Length of array mem = len(line in bytes) * y resolution
	array_length = v_screen.yres_virtual * f_screen.line_length;

	//Returns address of new mapping (void* cannot be deref)
	file_addr = mmap(NULL, array_length, PROT_READ | PROT_WRITE,
	MAP_SHARED, file_desc, 0);


	//Get terminal settings
	struct termios term;
	int ret_val3 = ioctl(0, TCGETS, &term);
	if(ret_val3 == -1){		//Check if error when getting
		write(2, "Error when getting terminal info.\n", 34);
	}

	//Unset ICANON and ECHO bits = OR bits, inverse, then AND bits
	term.c_lflag = term.c_lflag & ~(ICANON | ECHO);


	//Update terminal settings
	int ret_val4 = ioctl(0, TCSETS, &term);
	if(ret_val4 == -1){		//Check if errored when setting
		write(2, "Error when setting terminal settings.\n", 38);
	}

	//Clear screen of all text
	clear_screen();
}



/*
Purpose: This function will undo whatever it is that needs to be cleaned
	 up before the program exits.
Syscall: ioctl()
Parameters: None
Return: None
*/
void exit_graphics(void){

	//Get information on terminal
	struct termios term;
	int ret_val1 = ioctl(0, TCGETS, &term);
	if(ret_val1 == -1){	//Check if errored getting terminal info
		write(2, "Error when getting terminal info.\n", 34);
	}

	//Reset ICANON and ECHO bits
	term.c_lflag = term.c_lflag | (ICANON | ECHO);

	//Update terminal settings
	int ret_val2 = ioctl(0, TCSETS, &term);
	if(ret_val2 == -1){	//Check if errored when resetting terminal
		write(2, "Error when resetting terminal.\n", 31);
	}

	//Close the file descriptor
	close(file_desc);

	//Unmap memory
	munmap(file_addr, array_length);

	//Clear screen of all squares
	clear_screen();
}



/*
Purpose: This function will clear the screen.
Syscall: write()
Parameters: None
Return: None
*/
void clear_screen(void){

	int ret_val = write(1, "\033[2J", 4);
	if(ret_val == -1){	//Check if errored when writing to stdout
		write(2, "Error when clearing screen.\n", 28);
	}
}



/*
Purpose: This function will get user input.
Syscalls: read(), select()
Parameters: None
Return: None
*/
char getkey(void){

	//get timeval struct setup
	struct timeval timer;
	timer.tv_sec = 0;
	timer.tv_usec = 0;

	//Add file description to set
	fd_set read_set;
	FD_SET(0, &read_set);

	//Wait for file to become ready for some I/O operation
	int ret_val1 = select(0+1, &read_set, NULL, NULL, &timer);
	if(ret_val1 == -1){	//Check if select() errored
		write(2, "Error with select().\n", 21);
	}

	//Read from a file descriptor (stdin) one char (1 byte)
	char c;
	int byte_read = read(0, &c, 1);
	if(byte_read == -1){	//Check if read() errors
		write(2, "Error when reading char.\n", 25);
	}
	return c;
}



/*
Purpose: This function will make the program sleep between frames
	 of graphics being drawn.
Syscalls: nanosleep()
Parameters:
	ms: number of milliseconds to sleep; long type
Return: None
*/
void sleep_ms(long ms){
	int s = ms / 1000;		  //Calculate seconds
	long ns = (ms % 1000) * 1000000;  //Calculate nanoseconds
	struct timespec t = {s, ns};	  //Initialize struct
	nanosleep(&t, NULL);		  //nanosleep syscall
}



/*
Purpose: This function will draw the letters.
Syscalls: None
Parameters:
	x - x-coordinate of the pixel; int type
	y - y-coordinate of the pixel; int type
	color - color to set pixel; color_t type
Return: None
*/
void draw_pixel(int x, int y, color_t color){

	int position = (y * line_len) + (x * bits_per_pixel / 8);
	color_t* pixel = (file_addr+position);
	*pixel = color;
}



/*
Purpose: This function will use draw_pixel() to draw a rectangle.
	 NOTE: This function will prevent the square from wrapping
	 around. It will not handle cases when the width and height
	 are too large of numbers that the topleft corner + width or
	 height leads to a square whose one or more edges are off the
	 screen.
Syscalls: None
Parameters:
	x1 - top left x-coordinate; int type
	y1 - top left y-coordinate; int type
	width - width of rectangle; int type
	height - height of rectange; int type
	c - color of the pixels; color_t type
Return: None
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){

	//Check if left side is off screen
	if(x1 < 0){
		x1 = 0;
	}

	//Check if right side is off screen
	if(x1 + width > screen_width - 1){
		x1 = screen_width - 1 - width;
	}

	//Check if top side is off screen
	if(y1 < 0){
		y1 = 0;
	}

	//Check if bottom side is off screen
	if(y1 + height > screen_height - 1){
		y1 = screen_height - 1 - height;
	}

	int i;
	int j;
	//Draw Rectangle
	for(i = 0; i < height; i++){		//y-axis
		for(j = 0; j < width; j++){	//x-axis
			draw_pixel(x1 + j, y1 + i, c);
		}
	}
}



/*
Purpose: Draw the ASCII character at position (x,y).
Syscall: None
Parameters:
	x - x-coordinate; int type
	y - y-coordinate; int type
	c - ASCII character to draw; char type;
Return: None
*/
void draw_char(int x, int y, char c, color_t color){

	int num_rows = 16;		//Num rows per ASCII char
	int offset = (int)c * num_rows;	//Start of ASCII char
	int i, j;

	for(i = 0; i < num_rows; i++){	 //Rows in ASCII letter
		for(j = 0; j < 8; j++){  //Columns in ASCII letter
			//If bit is turned on, draw pixel
			if(iso_font[offset + i] & (0x01 << j)){
				draw_pixel(x+j, y+i, color);
			}
		}
	}
}



/*
Purpose: This function draws the string with the specified color at the
	 starting location (x,y).
Syscall: None
Parameters:
	x - top left x-coordinate; int type
	y - top left y-coordinate; int type
	text - text string; const char pointer
	c - color of the pixels; color_t type
Return: None
*/
void draw_text(int x, int y, const char *text, color_t c){

	int pos = 0;
	int char_width = 8;		//ASCII char has 8 px wide
	int char_height = 16;		//ASCII char has 16 px height

	while(text[pos] != '\0'){

		//Check if characters off right side of screen
		if(x+char_width > screen_width-1){
			x = 0;	    		//Move to left side
			y = y + char_height;	//Next row
		}

		//Check if letters will be off screen
		if(y + char_height > screen_height-1){
			y = 0;			//Move to top row
		}

		//Draw character
		draw_char(x, y, text[pos], c);

		//Update position
		x = x + char_width;
		pos = pos + 1;
	}
}
