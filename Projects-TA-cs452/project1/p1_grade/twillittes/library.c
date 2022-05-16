/*
Author: Taylor Willittes
Project 1
Purpose:Drawing rectangles and letters on screen
*/

#include <termios.h>
#include <linux/fb.h>
#include <time.h>
#include <stdlib.h>
#include "iso_font.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <inttypes.h>
typedef unsigned short color_t;

FILE* filePointer = NULL;
char* temp = NULL;
size_t size = 0;


int one;
int two;

void clear_screen()
{
//write
write(STDIN_FILENO, "\033[2J", 8);
}

void init_graphics()
{
//open, ioctl, mmap syscalls used
filePointer = open("/dev/fb0", O_RDWR);
struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;
int t = ioctl((int) filePointer, FBIOGET_VSCREENINFO, &var);
int t2 = ioctl((int) filePointer, FBIOGET_FSCREENINFO,&fix); 
//printf("%d %d\n", t, t2);
void* address = NULL;
//printf("%d %d\n", var.yres_virtual, fix.line_length);
size = var.yres_virtual * fix.line_length;


temp = (char*) mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, (int) filePointer, 0);
one = var.bits_per_pixel/8;
two = fix.line_length;

struct termios term;
//ioctl(STDIN_FILENO, TCGETS, &term);
tcgetattr(STDIN_FILENO, &term);
term.c_lflag &= ~(ICANON | ECHO);
tcsetattr(STDIN_FILENO, TCSANOW, &term);
//ioctl((int) filePointer, TCSETS, &term); 
clear_screen();
}

void exit_graphics()
{
clear_screen();
//ioctl sys call
close(filePointer);
munmap(temp, size);
struct termios term2;
//ioctl((int) filePointer, TCGETS, &term2);
tcgetattr(STDIN_FILENO, &term2);
term2.c_lflag |= (ECHO | ICANON);
tcsetattr(STDIN_FILENO, TCSANOW, &term2);
//ioctl((int) filePointer, TCSETS, &term2);

}

//void clear_screen()
//{
//write
//write(STDIN_FILENO, "\033[2J", 4);
//}

char getkey()
{
//select, read
fd_set set;
struct timeval t;
FD_ZERO(&set);
FD_SET(STDIN_FILENO, &set);
t.tv_sec = 0;
t.tv_usec = 0;
if (select(STDIN_FILENO + 1, &set, NULL, NULL, &t))
{
char c;
read(STDIN_FILENO, &c, 1);
//printf("K: %c\n", c);
return c;
}

}

void sleep_ms(long ms)
{
struct timespec time;
time.tv_sec = 0;
time.tv_nsec = 1000000 * ms;
nanosleep(&time, NULL);
}

void draw_pixel(int x, int y, color_t color)
{
if (y < 0)
{
y = 5;
}
*((color_t*)(temp+(x*one+y*two))) = color;
}

void draw_rect(int x1, int y1, int width, int height, color_t c)
{
//draw_pixel(320, 240, c);
//draw_pixel(320, 241, c);
//draw_pixel(320, 242, c);
//printf("1\n");
//if (y1 > height)
//{
//y1 = height - 1;
//}
int a;
int b;
for (a = x1; a < x1 + width; a = a + 1)
{
  for (b = y1; b < y1 + height; b = b + 1)
  {
    draw_pixel(a, b, c);
  }
}
}

void draw_char(int x, int y, const char chara, color_t c)
{
int a = 0;
int b = 0;

for (a = 0; a < 16; a++)
{
unsigned char row = iso_font[chara*16+a];
for (b = 0; b < 8; b++)
{
row >>= b;
if (row & 1 == 1)
{
draw_pixel(x+b, y+a, c);
}
}//end inner for loop
}//end outer for loop
}//end func

void draw_text(int x, int y, const char *text, color_t c)
{
int temp = 0;
for (temp = 0; text[temp] != '\0'; temp++)
{
draw_char(x + temp*8, y, text[temp], c);
}
}

