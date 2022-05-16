#include <stddef.h>
#include <sys/mman.h>
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

void init_graphics();
void exit_graphics();
char getkey();
void sleep_ms(long ms);
void clear_screen();
void draw_pixel(int x, int y, color_t color);
void *create_buffer();
void draw_rect(int x1, int y1, int width, int height, color_t color);
