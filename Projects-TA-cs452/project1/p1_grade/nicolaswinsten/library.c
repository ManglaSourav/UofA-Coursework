#include "graphics.h"
#include "iso_font.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

color_t *framebuffer = NULL;
int width = 640;
int height = 480;

// initialize graphics access
void init_graphics() {
	// getting file descriptor for framebuffer
	int fd = open("/dev/fb0", O_CREAT | O_RDWR);
	
	// retrieving screen parameters
	struct fb_var_screeninfo vinfo;
	ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
	
	struct fb_fix_screeninfo finfo;
	ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
	
	size_t mmap_size = vinfo.yres_virtual * finfo.line_length;
	
	// map framebuffer to memory
	framebuffer = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		
	// disable keypress echo
	struct termios terminal_settings;
	ioctl(0, TCGETS, &terminal_settings);
	
	terminal_settings.c_lflag &= ~(ICANON | ECHO);
	
	ioctl(0, TCSETS, &terminal_settings);
	
}

// return terminal CLI to user
void exit_graphics() {
	// reenable keypress echo
	struct termios terminal_settings;
	ioctl(0, TCGETS, &terminal_settings);
	
	terminal_settings.c_lflag |= ICANON | ECHO;
	
	ioctl(0, TCSETS, &terminal_settings);
}

// clear the entire screen
void clear_screen() {
	write(0, "\033[2J", 4);
}

// return the char representing the key last pressed.
// if no key is pressed, return \0
char getkey() {
	fd_set rfds;
	struct timeval tv;
	int retval;
	char buf = '\0';
	
	// read from stdin
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	
	// no waiting
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	
	retval = select(1, &rfds, NULL, NULL, &tv);
	if (retval)
		read(0, &buf, sizeof(char));
		
	return buf;
}

// sleep the given number of milliseconds between frames
void sleep_ms(long ms) {
	struct timespec tm;
	tm.tv_sec = 0;
	tm.tv_nsec = ms * 1000000;
	nanosleep(&tm, NULL);
}

// color the pixel at x,y with the given color
void draw_pixel(int x, int y, color_t color) {
	// NOP if x,y is outside the graphics context
	if (x < 0 || x >= width || y < 0 || y >= height) return;
	*(framebuffer + y*width + x) = color;
}

// draw the outline of a rectangle whose top left corner is at x1,y1
void draw_rect(int x1, int y1, int width, int height, color_t color) {
	// draw the horizontal sides
	int x;
	for (x = x1; x < x1 + width; x++) {
		draw_pixel(x, y1, color);
		draw_pixel(x, y1 + height, color);
	}
	
	//draw the vertical sides
	int y;
	for (y = y1; y < y1 + height; y++) {
		draw_pixel(x1, y, color);
		draw_pixel(x1 + width, y, color);
	}
}

// fill a box at the given location with the color
void fill_rect(int x1, int y1, int width, int height, color_t color) {
	int y;
	int x;
	for (y = y1; y < y1 + height; y++) {
		for (x = x1; x < x1 + width; x++) {
			draw_pixel(x, y, color);
		}
	}
}

// draw the given char using iso_font
void draw_char(int x, int y, char c, color_t color) {	
	int row;
	int col;
	for (row = 0; row < 16; row++) {
		char row_enc = *(iso_font + c*16 + row);
		for (col = 0; col < 8; col++) {
			if (1 & (row_enc >> col))
				draw_pixel(x + col, y + row, color);
		}
	}
	
}

// draw the given string in iso_font
void draw_text(int x, int y, const char *text, color_t color) {
	int i = 0;
	while (*(text+i) != '\0') {
		draw_char(x + i*8, y, *(text+i), color);
		i++;
	}
}



// encode the given r g and b values into a single rgb color_t (16 bits)
// r and b channels have range [0,31], g channel has range [0,63]
// so yellow would be rgb(31, 63, 0)
color_t rgb(unsigned int r, unsigned int g, unsigned int b) {
	int colors[] = {r, g, b};
	// clamp the colors to [0,31]
	int i;
	for (i = 0; i < 3; i = i + 2) {
		if (colors[i] < 0) colors[i] = 0;
		if (colors[i] > 31) colors[i] = 31;
	}
	if (colors[1] < 0) colors[1] = 0;
	if (colors[1] > 63) colors[1] = 63;
	return (colors[0] << 11) | (colors[1] << 5) | colors[2];
}


