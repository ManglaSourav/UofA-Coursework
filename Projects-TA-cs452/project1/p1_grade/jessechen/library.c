#include <fcntl.h>
#include "iso_font.h"
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
int fd;
struct termios backup; // used to backup previous termios 
struct termios termios;
fd_set fdSet;
fd_set fdSetBackup;
typedef uint16_t color_t;
char* mmapAddress;
unsigned int xresVirt;
unsigned int yresVirt;
unsigned int fileSize;
const int letterX = 8;
const int letterY = 16;
const int padding = 16;
int lineLen;

// Drawing functions ===============================================================

void draw_pixel(int x, int y, color_t c) {
	// make sure the pixel doesn't go off the screen
	if (x >= 0 && x < lineLen && y >= 0 && y < yresVirt) {
		// offset from arr[0]
		mmapAddress[lineLen * y + 2 * x] = c;
	}
}

void draw_rect(int x, int y, int w, int h, color_t c) {
	// make sure the rect doesn't go off the screen
	if (x + w < lineLen && y + h < yresVirt && x >= 0 && y >= 0) {
		int i;
		int j;
		for (i = x; i < x + w; i++) {
			for (j = y; j < y + h; j++) {
				draw_pixel(i, j, c);
			}
		}
	}
}

// gets the nth bit in the 16 1-byte int char
unsigned int getNthBit(int n, unsigned char ch) {
	return (1 & (ch >> (n - 1)));
}

void draw_letter(int x, int y, char ch, color_t c, int xShift) {
	unsigned char chArr[letterY];
	int i;
	int j;

	// get letter info from iso_font
	for (i = 0; i < letterY; i++) {
		chArr[i] = iso_font[ch * letterY + i];
	}

	// draw pixels
	for (j = 0; j < letterY; j++) {
		for (i = 0; i < letterX; i++) {
			if (getNthBit(i, chArr[j])) {
				draw_pixel(i + x + xShift * letterX, j + y, c);
			}
		}
	}
}

void draw_text(int x, int y, const char* text, color_t c) {
	int i = 0;
	while (text[i] != '\0') {
		draw_letter(x, y, text[i], c, i);
		i += 1;
	}
}
// ==================================================================================

// syscall(s): nanosleep
void sleep_ms(long ms) {
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = ms * 1000000;
	nanosleep(&ts, NULL);
}

// syscall(s): write
void clear_screen() {
	write(STDOUT_FILENO, "\033[2J", 4 * sizeof(char));
}
// syscall(s): open, ioctl, mmap
void init_graphics() {

	fd = open("/dev/fb0", O_RDWR);

	struct fb_var_screeninfo fbVarScreeninfo;
	struct fb_fix_screeninfo fbFixScreeninfo;
	ioctl(fd, FBIOGET_VSCREENINFO, &fbVarScreeninfo);
	ioctl(fd, FBIOGET_FSCREENINFO, &fbFixScreeninfo);

	xresVirt = fbVarScreeninfo.xres_virtual;
	yresVirt = fbVarScreeninfo.yres_virtual;
	lineLen = fbFixScreeninfo.line_length;
	fileSize = yresVirt * lineLen;
	mmapAddress = mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	// https://stackoverflow.com/questions/32809983/how-do-i-use-the-ioctl-system-call-to-disable-keypress-echo-and-buffering
	ioctl(STDIN_FILENO, TCGETS, &termios);
	backup = termios;
	termios.c_lflag &= ~(ICANON | ECHO);
	ioctl(STDIN_FILENO, TCSETS, &termios);
	clear_screen();

}

// syscall(s): ioctl
void exit_graphics() {
	clear_screen();
	draw_text(225, yresVirt / 2, "You have exit the program.", 255);
	draw_text(225, yresVirt / 2 + 16, "Copyright (C) Feb 6, 2022", 255);
	ioctl(STDIN_FILENO, TCSETS, &backup);
	munmap(mmapAddress, fileSize);
	close(fd);
}

// syscall(s): select, read
// referenced: https://www.youtube.com/watch?v=Y6pFtgRdUts&t=288s
char getkey() {
	FD_ZERO(&fdSetBackup);
	FD_SET(STDIN_FILENO, &fdSetBackup); // add STDIN_FILENO into our fd_set
	fdSet = fdSetBackup; // b/c select is destructive

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	int result = select(1, &fdSet, 0, 0, &tv);
	if (result > 0) {
		char key;
		read(STDIN_FILENO, &key, sizeof(char));
		return key;
	} else if (result == -1) {
		exit(EXIT_FAILURE);
	}
}







