#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>


typedef unsigned short color_t;
#define RGB(r,g,b) (r/(g<<6)/(b<<11))

void *buff;
int fd;
size_t length;
struct termios term;
struct termios newterm;
struct fb_var_screeninfo vi;
struct fb_fix_screeninfo fi;

void init_graphics() {
	fd = open("/dev/fb0", O_RDWR);

	if (fd < 0) {
		_exit(1);
	}

	if ((length = ioctl(fd, FBIOGET_VSCREENINFO, &vi)) < 0) {
		close(fd);
		_exit(2);
	}

	if ((length = ioctl(fd, FBIOGET_FSCREENINFO, &fi)) < 0) {
		close(fd);
		_exit(3);
	}
	buff = mmap(NULL, fi.smem_len, PROT_READ | PROT_WRITE, 
	MAP_SHARED,fd, 0);

	ioctl(STDIN_FILENO, TCGETA, &term);
	newterm = term;

	newterm.c_lflag &= ~(ICANON | ECHO);

	ioctl(STDIN_FILENO, TCSETA, &newterm);
}

void exit_graphics() {
	ioctl(STDIN_FILENO, TCSETA, &term);
	close(fd);
	munmap(buff, length);
}

char getkey() {
	fd_set rfds;
	struct timeval tv;
	int retval;
	char* input = NULL;
	
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	retval = select(0, &rfds, NULL, NULL, &tv);
	if (retval) {
		read(0, input, 1);
		return(*input);
	}
	return(' ');
}
void sleep_ms(long ms) {
	nanosleep((const struct timespec[]){{0, (ms*1000000)}}, NULL);
}

void* create_buffer() {
	return mmap(NULL, fi.smem_len, PROT_READ | PROT_WRITE, 
	MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void clear_screen() {
	write(1, "\033[2J", 7);
}

void draw_pixel(int x, int y, color_t color) {
	int location = (y * 640 + x) * 2;
	color_t* pix = (color_t*)(buff + location);
	*pix = color;
}
void draw_rect(int x1, int y1, int width, int height, color_t c)
{
	int y, x;
	for (x = x1; x < (x1 + width); x++)
	{
		for (y = y1; y < (y1 + height); y++)
		{
			draw_pixel(x,y,c);
		}
	}
}
