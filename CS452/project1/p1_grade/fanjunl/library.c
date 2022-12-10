#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <termios.h>  
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include "iso_font.h"
#include "graphics.h"
color_t* fb0_addr;
fd_set readfd;
struct timeval timeout;
int screen_fbd = 0;
int ttyd = 0;
unsigned fb_size;
struct fb_fix_screeninfo fb_fix;
struct fb_var_screeninfo fb_var;
struct termios term;
//--------------------------



void init_graphics() {
	char* env = NULL;
	char* env1 = NULL;
	env = "/dev/fb0";
	env1 = "/dev/tty";
	if ((screen_fbd = open(env, O_RDWR)) < 0) {
		//printf("errno=%d\n", errno);
	}
	if ((ttyd = open(env1, O_RDWR)) < 0) {
		//printf("errno=%d\n", errno);
	}

	ioctl(screen_fbd, FBIOGET_FSCREENINFO, &fb_fix);
	//printf("fb_fix.line_lent=%d\n", fb_fix.line_length);
	//printf("fb_fix.accel=%d\n", fb_fix.accel);
	ioctl(screen_fbd, FBIOGET_VSCREENINFO, &fb_var);
	//printf("fb_var.xres=%d\n", fb_var.xres);
	//printf("fb_var.yres=%d\n", fb_var.yres);
	//printf("fb_var.bits_per_pixel=%d\n", fb_var.bits_per_pixel);
	fb_size = fb_var.yres * fb_fix.line_length;
	//printf("fb_size=%d\n", fb_size);
	fb0_addr = (color_t*)mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, screen_fbd, 0);
	ioctl(ttyd, TCGETS, &term);
	term.c_lflag &= ~ICANON;
	term.c_lflag &= ~ECHO;
	ioctl(ttyd, TCSETS, &term);

}
int isOver(int x, int y) {
	if (x > 639 || x < 0) {
		return 0;
	}

	if (y > 439 || y < 0) {
		return 0;
	}
	return 1;
}
void draw_pixel(int x, int y, color_t color) {
	if (isOver(x, y)) {
		*(fb0_addr + x + y * 640) = color;
	}
}
void draw_rect(int x1, int y1, int width, int height, color_t c) {
	size_t i, j;

	for (i = 0; i < width; i++)
	{
		for (j = 0; j < height; j++)
		{
			draw_pixel(x1 + i, y1 + j, c);
		}
	}
}
void exit_graphics() {
	struct termios term;
	ioctl(ttyd, TCGETS, &term);
	term.c_lflag |= ECHO;
	term.c_lflag |= ICANON;
	ioctl(ttyd, TCSETS, &term);
	close(ttyd);
	close(screen_fbd);
	munmap((void*)fb0_addr, fb_size);
}
void clear_screen() {
	write(ttyd, "\033[2J", 5);
}
char getkey() {
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	int ret;
	char value;
	FD_ZERO(&readfd);                      
	FD_SET(ttyd, &readfd);               
	ret = select(ttyd + 1, &readfd, NULL, NULL, &timeout);
	if (FD_ISSET(ttyd, &readfd)) {        
		read(ttyd, &value, 1);
	}

	return value;
}
void sleep_ms(long ms) {
	struct  timespec  req, rem;
	long sec = (int)(ms / 1000);
	ms = ms - (sec * 1000);
	req.tv_sec = sec;
	req.tv_nsec = ms * 1000000;
	while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
		continue;
	}

}

void draw_char(int x, int y, char c, color_t co) {
	int i, j;
	for (i = 0; i < 16; i++)
	{

		////printf("%d\n", *(iso_font + i + 16 * c));
		for (j = 0; j < 8; j++)
		{
			if (((*(iso_font + i + 16 * c) >> (7 - j)) & 1)) {
				draw_pixel(x + (7 - j), y + i, co);

			}
			//	//printf("%d ", ((*(iso_font + i + 16 * c) >> (7 - j)) & 1));
		}
		////printf("\n");


	}
}

void draw_text(int x, int y, const char* text, color_t co) {
	char* temp = (char*)text;
	int i = 0;
	while (*temp != '\n' && *temp != '\0')
	{
		draw_char(x + 10 * i, y, *temp, co);
		temp++;
		i++;
	}
}
