/*
	author: Oliver Lester
	description: This function will act as a small graphics library
	that performs basic graphical operations.
*/

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <linux/fb.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include "graphics.h"
#include "iso_font.h"

int gf;
void *map;
int screensize;
struct fb_var_screeninfo varinfo;
struct fb_fix_screeninfo fixinfo;
struct termios old_term;
struct termios new_term;
int temp;

/*
	This function acts as the sets up all the necessary things to
	make to actual work the graphics. This includes mapping the
	framebuffer and disabling keypress echos.
*/
void init_graphics() {
	gf=open("/dev/fb0",O_RDWR);
	ioctl(gf, FBIOGET_VSCREENINFO, &varinfo);
	ioctl(gf, FBIOGET_FSCREENINFO, &fixinfo);
	screensize = varinfo.yres_virtual * fixinfo.line_length;
	map = mmap(NULL, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, gf, 0);
	temp = ioctl(0, TCGETS, &old_term);
	new_term = old_term;
	new_term.c_lflag &= ~ECHO;
	new_term.c_lflag &= ~ICANON;
	temp = ioctl(0, TCSETS, &new_term);
}

/*
	This function reenables the keypress echo and unmaps the
	framebuffer.
*/
void exit_graphics() {
	temp = ioctl(0, TCSETS, &old_term);
	munmap(map,0);
	close(gf);
}

/*
	This function clears the screen;
*/
void clear_screen() {
	write(1, "\033[2J", 7);
}

/*
	This function reads in a press from the keyboard and returns the
	character pressed.
*/
char getkey() {
	char buf[8];
	fd_set r_fd, w_fd, e_fd;
	FD_ZERO(&r_fd);
	FD_ZERO(&w_fd);
	FD_ZERO(&e_fd);
	FD_SET(0, &r_fd);
	select(1, &r_fd, &w_fd, &e_fd, NULL);
	read(0,buf,8);
	return buf[0];
}

/*
	This function takes in a long that is to represent an ammount of
	milliseconds. And puts the things to sleep for that ammount of
	times.
*/
void sleep_ms(long ms) {
	struct timespec t;
	t.tv_sec = (int) (ms/1000);
	t.tv_nsec = (ms%1000) * 1000000;
	nanosleep(&t, NULL);
}

/*
	This function takes in a coordinate and and changes the color at
	that specific location in the mapped framebuffer.
*/
void draw_pixel(int x, int y, color_t color) {
	int location =  x * 2 + y * fixinfo.line_length;
	color_t *pixel = (map + location);
	*pixel = color;
}

/*
	This function takes in a coordinate and width and heights for a
	rectangle. It then uses the draw_pixel function to make a
	rectangle of the given color.
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	int i;

	for (i = 0; i < width; i++) {
		draw_pixel(x1++, y1, c);
		draw_pixel(x1, y1+height, c);
	}

	for (i = 0; i < height; i++) {
		draw_pixel(x1, y1++, c);
		draw_pixel(x1 - width, y1, c);
	}

}

/*
	This is an extra function, which fundamentally works like
	draw_rect but blacks out the entire are specified.
*/
void erase(int x1, int y1, int width, int height) {
	int i;
	int j;

	for (i = 0; i < width; i++) {
		for (j = 0; j < height; j++) {
			draw_pixel(x1 + i, y1 + j, 0);
		}
	}
}

/*
	This function has the goal of writing a string to the
	framebuffer. Though, it simply loops through the given string
	and passes all of them to the draw_letter function. The string
	starts at the given coordinates and of the given color.
*/
void draw_text(int x, int y, const char *text, color_t c) {
	while (text[0] != '\0') {
		draw_letter(x, y, text[0], c);
		text++;
		x = x + 8;
	}
}

/*
	This function locates an spot in an area. It then writes the
	8x16 letter to the screem.
*/
void draw_letter(int x, int y, char ch, color_t c) {
	int i = ch * 16;
	int j;
	int cur;
	for (i = ch * 16; i < ch * 16 + 16; i++) {
		cur = iso_font[i];
		for (j = 0; j < 8; j++) {
			if (cur >= (1<<(7-j))) {
				draw_pixel(x+(7-j), y, c);
				cur = cur - (1 << (7-j));
			}
		}
		y++;
	}
}
