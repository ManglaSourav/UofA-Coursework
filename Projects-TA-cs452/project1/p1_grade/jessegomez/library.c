#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include "iso_font.h"

typedef unsigned short color_t;
int framebufferFileDescriptor;
struct fb_var_screeninfo vscreeninfo;
struct fb_fix_screeninfo fscreeninfo;
struct termios termInfo;
int* fbp;

int yResolution;
int xResolution;

void init_graphics(){
	framebufferFileDescriptor = open("/dev/fb0", O_RDWR);
	int vscreeninfoReturn = ioctl(framebufferFileDescriptor, FBIOGET_VSCREENINFO, &vscreeninfo);
	if (vscreeninfoReturn ==-1) {
		exit(2);
	}
	
	int fscreeninfoReturn = ioctl(framebufferFileDescriptor, FBIOGET_FSCREENINFO, &fscreeninfo);
	
	yResolution = vscreeninfo.yres_virtual;
	xResolution = vscreeninfo.xres_virtual;
	int termiosReturn = ioctl(0, TCGETS, &termInfo);
	
	//termInfo.c_lflag &= ~(ICANON|ECHO);
	termiosReturn = ioctl(0, TCSETS, &termInfo);
	fbp = mmap(NULL, fscreeninfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, framebufferFileDescriptor, 0);
}

void exit_graphics() {
	termInfo.c_lflag &= (ICANON|ECHO);
	int termiosReturn = ioctl(0, TCSETS, &termInfo);
}

void clear_screen() {
	write(0, "\033[2J", 64);
}

char getkey() {
	fd_set rfds;
	struct timeval tv;
	FD_ZERO(&rfds);
    FD_SET(0, &rfds);
	int keyReturn = select(1, &rfds, NULL, NULL, &tv);
	char value;
	if (keyReturn) {
		int readReturn = read(0, &value, 1);
	}
	return value;
}

void sleep_ms(long ms){
	struct timespec ts = {
       (int)(ms / 1000),     /* secs (Must be Non-Negative) */ 
       (ms % 1000) * 1000000 /* nano (Must be in range of 0 to 999999999) */ 
	};
   nanosleep(&ts , NULL);
}

void draw_pixel(int x, int y, color_t color){
	int location = (x) * (vscreeninfo.bits_per_pixel/8) + (y) * fscreeninfo.line_length;
	
	fbp[location] = (unsigned short) color;
}

void draw_rect(int x1, int y1, int width, int height, color_t c){
	int x;
	for (x=x1; x <= x1+width; x++) {
		draw_pixel(x, y1, c);
		draw_pixel(x, y1+height, c);
	}
	int y;
	for ( y=y1; x <= y1+height; y++) {
		draw_pixel(x1, y, c);
		draw_pixel(x1+width, y, c);
	}
}

void draw_character(int x, int y, char c, color_t color) {
	int startIso = ((int) c) * 16;
	int row;
	int word;
	for ( row=0; row<16;row++){
		for ( word=0; word < 16; word++) {
			if ((0x40 >> word) & iso_font[startIso+row] > 0) {
				draw_pixel(x+word, y+row, color);
			}
		}
	}
}

void draw_text(int x, int y, const char *text, color_t c) {
	int index=0;
	while (*text != '\0') {
		draw_character(x+index*16,y, *text, c);
		index++;
		text++;
	}
}



