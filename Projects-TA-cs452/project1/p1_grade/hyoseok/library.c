/*
*	AUTHOR: Hyoseo Kwag
*	DATE: Spring 2022 Assignment 1
*	DESCRIPTION: Graphics library that draws pixels on screen.
*/

#include "graphics.h"	// my header file
#include<stdio.h>	// allow printf()
#include<inttypes.h>	// allow int types
#include<fcntl.h>	// allow open()
#include<unistd.h>	// allow size_t
#include<termios.h>	// allow terminal I/O
#include<stdlib.h>	// allow exit()
#include "iso_font.h"	// text font file

#include<linux/fb.h>
#include<sys/mman.h>
#include<sys/ioctl.h>

// these are global variables
int framebuffer;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
char *fbmmap;
size_t fb_screensize;
int fbmmap_size;
struct termios old_echo, new_echo;


void init_graphics() {
//	printf("init_graphics----------------------------\n");

	// open a framebuffer
	framebuffer = open("/dev/fb0", O_RDWR);
//	printf("framebuffer: %d\n", framebuffer);

	// get the virtual resolution of current screen
	ioctl (framebuffer, FBIOGET_VSCREENINFO, &vinfo);
	//ioctl (framebuffer, FBIOPUT_VSCREENINFO, &vinfo);
	int fb_width = vinfo.xres;
	int fb_height = vinfo.yres;
	int fb_bpp = vinfo.bits_per_pixel;
	int fb_yres_virt = vinfo.yres_virtual; // gives height (480)
///	printf("fb dimension: %d width, %d height\n", fb_width, fb_height);
//	printf("fb per pixel: %d bits, %d bytes\n", fb_bpp, fb_bpp/8);

	// get the bit depth of current screen
	ioctl (framebuffer, FBIOGET_FSCREENINFO, &finfo);
	int fb_line_len = finfo.line_length;
	unsigned long fb_smem_start = finfo.smem_start;
	int fb_smem_len = finfo.smem_len;
//	printf("fb line length: %d\n", fb_line_len);
//	printf("fb start address: %lu\n", fb_smem_start);
//	printf("fb length: %d\n\n", fb_smem_len);

	// memory mapping the resolution pointers
	fb_screensize = fb_width * fb_height * fb_bpp/8;
	fbmmap = mmap(0, fb_screensize, PROT_READ | PROT_WRITE, 
	MAP_SHARED, framebuffer, 0);
	if (fbmmap == MAP_FAILED) {
		perror("ERROR! Failed to map framebuffer.\n");
		exit(1);
	}
	fbmmap_size = fb_yres_virt * fb_line_len;
//	printf("fb screen size: %d\n", fb_screensize);
//	printf("memory mapping: %p\n", fbmmap);
//	printf("size of mmap: %d\n\n", fbmmap_size);

	// disable keypress echo & buffer keypresses
	ioctl(0, TCGETS, &old_echo);
	new_echo = old_echo;
	new_echo.c_lflag &= ~(ICANON | ECHO);
	new_echo.c_cc[VMIN] = 1;
	new_echo.c_cc[VTIME] = 0;
	ioctl(0, TCSETS, &new_echo);
}

void exit_graphics() {
//	printf("exit_graphics-----------------------------\n");

	// close file
	close(framebuffer);
//	printf("Closed frame buffer.\n");

	// unmap memory
	munmap(fbmmap, fb_screensize);
//	printf("Unmapped memory.\n");

	// reenable key press echoing & buffering
	ioctl(0, TCSETS, &old_echo);
//	printf("Reenabled echo.\n");
}

void clear_screen() {
//	printf("clear_screen-------------------------------\n");

	// use ANSI escape code
	printf("\033[2J");
}

char getkey() {
//	printf("getkey------------------------------------\n");

	// do not wait for input; if input, return the input
	struct timeval time;
	time.tv_usec = 0;
	time.tv_sec = 0;
	fd_set fdset;
	char input;
	FD_ZERO(&fdset);
	FD_SET(0, &fdset);
	int selected = select(1, &fdset, NULL, NULL, &time);

	if (selected == 0) {
		// no input
//		printf("waited, no input...\n");
	} else if (selected == -1) {
		// failed to select
//		printf("failed to select!\n");
	} else {
		// selected == 1; input detected
		read(0, &input, 1);
//		printf("input -> %c\n", input);
	}
	return input;
}

void sleep_ms(long ms) {
//	printf("sleep_ms--------------------------------\n");
	// sleep for ms (given number) milliseconds
	struct timespec sleep_for = {ms/1000, ms*1000000};
//	printf("sleeping for %d seconds...\n", ms/1000);
	nanosleep(&sleep_for, NULL);
//	printf("woke up!\n\n");
}

void draw_pixel(int x, int y, color_t color) {
//	printf("draw_pixel----------------------------------\n");

//	printf("x: %d, y: %d, color: %d\n", x, y, color);

	// find address of coordinate
	long address = (x + vinfo.xoffset) *
	(vinfo.bits_per_pixel/8) + (y + vinfo.yoffset) * 
	finfo.line_length;
//	printf("address: %d\n", address);

	// get rgb of color from uint16_t
	unsigned r = (color & 0xF800) >> 11;
	unsigned g = (color & 0x07E0) >> 5;
	unsigned b = (color & 0x1F);
	//printf("rgb: %d %d %d\n", r, g, b);

	// draw pixel
	unsigned short int pixel = r<<11 | g<<5 | b;
	*((unsigned short int*)(fbmmap + address)) = pixel;
}

void draw_rect(int x1, int y1, int width, int height, color_t c) {
//	printf("draw_rect------------------------\n");

	// draw top horizontal line of rect
	int i;
	for (i=x1; i<x1+width; i++) {
		draw_pixel(i, y1, c);
	}

	// draw bottom horizontal line of rect
	int j;
	for (j=x1; j<x1+width; j++) {
		draw_pixel(j, y1+height, c);
	}

	// draw left vertical line of rect
	int k;
	for (k=y1; k<y1+height; k++) {
		draw_pixel(x1, k, c);
	}

	// draw right vertical line of rect
	int l;
	for (l=y1; l<y1+height; l++) {
		draw_pixel(x1+width, l, c);
	}
}

void draw_text(int x, int y, const char *text, color_t c) {
//	printf("draw_text-----------------------------\n");

//	printf("text to draw: %s\n", text);

	int i=0;
	int next_char=0;
	while (text[i] != '\0') {
		draw_char(x+next_char, y, text[i], c);
		i++;
		next_char+=8;
	}
}

void draw_char(int x, int y, char c, color_t color) {
//	printf("draw_char--------------------------\n");

//	printf("coordinate: %d %d, curr: %c, color: %d\n",x,y,c,color);
	int first_indice = c*16;
	int second_indice = c*16+15;
//	printf("indices %d - %d\n", first_indice, second_indice);

	int i;
	int y_inc = 0;
	for (i=first_indice; i<second_indice; i++) {
//	for (i=first_indice+1; i<first_indice+3; i++) {
		int curr_font = iso_font[i];
//		printf("font: %d\n", curr_font);
		int index = 7;	// 8 bytes - 1
		int bin[8];
		while (index >=0) {
			bin[index] = curr_font & 1;
			if (bin[index] == 1) {
//				printf("index %d; y_inc %d\n", index, 
//				y_inc);
				draw_pixel(x+(8-index), y+y_inc, color);
			}

			index--;
			curr_font >>= 1;
		}
		y_inc++;
	}
}





// main for testing
//int main(void) {
//	init_graphics();
//	getkey();
//	sleep_ms(5000);	// sleeping for 5 seconds
//	int i;
//	for (i=400; i<639; i++) {
//		draw_pixel(i, 10, 30);
//		draw_pixel(i, 11, 30);
//	}

//	draw_rect(400,50,100, 50, 30);

//	draw_text(400,100,"hello world!",30);
//	draw_text(400,200,"ABCDEFGHIJKLMNOP",30);

//	clear_screen();
//	exit_graphics();
//}
