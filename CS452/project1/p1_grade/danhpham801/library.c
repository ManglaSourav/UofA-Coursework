#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <linux/fb.h>
#include <inttypes.h>
#include <termios.h>
#include <sys/select.h>
#include <time.h>
#include <iso_font.h>

struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;
char *map;
size_t size;
int fBuff;
typedef unsigned short color_t;
struct termios od, nw;

void init_graphics(){
	fBuff = open("/dev/fb0", O_RDWR);
	ioctl(fBuff, FBIOGET_VSCREENINFO, &vinfo);
	ioctl(fBuff, FBIOGET_FSCREENINFO, &finfo);
	size = vinfo.yres * finfo.line_length;
	map = mmap(0,size,PROT_READ|PROT_WRITE,MAP_SHARED,fBuff,0);
	ioctl(0, TCGETS, &od);
	nw = od;
	nw.c_lflag &= ~(ICANON|ECHO);
	ioctl(0, TCSETS, &nw);
}

color_t get_color_t(int r, int g, int b){
	color_t ret = (r<<11)|(g<<5)|(b);
	return ret;
}

void exit_graphics(){
	close(fBuff);
	munmap(map, size);
	ioctl(0, TCSETS, &od);
}

void clear_screen(){
	printf("\033[2J");
}

char getkey(){
	fd_set set;
	struct timeval tv;
	int retval, ret;
	char buff[1];
	FD_ZERO(&set);
	FD_SET(0, &set);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	retval = select(8, &set, NULL, NULL, &tv);
	if(retval != 0){
		ret = read(0,(void*)buff, 1);
		if(ret !=-1){
			return buff[0];
		}
	}
	return 0;
}

void sleep_ms(long ms){
	struct timespec tv = {ms/1000, ms*1000000};
	nanosleep(&tv, NULL);
}

void draw_pixel(int x, int y, color_t c){
	int location = x*(vinfo.bits_per_pixel/8) +
	y*finfo.line_length;
	*((int*) (map + location)) = c;
}

void draw_rect(int x, int y, int width, int height, color_t c){
	int i , j;
	for(i=y; i<= y+height; i++){
		for(j=x;j<= x+width;j++){
			draw_pixel(j,i,c);
		}
	}
}

void draw_text(int x, int y, const char* text, color_t c){
	int chr;
	for(;*text != '\0'; text++){
		//chr = (int) *text;
		int i, ii, x1 = x*8, y1 = y* (ISO_CHAR_HEIGHT -1);

		for(i=0;i<15;i+=1){
			int sys = iso_font[*text *16+i];
			int px = x1, py = y1+i;

			for(ii=0;ii<8;ii++){
				px++;
				if( (sys & (1 << ii)) ){
					draw_pixel(px,py,c);
				}
			}
		}
		x+=1;
	}
}
