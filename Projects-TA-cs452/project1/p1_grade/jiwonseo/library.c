/*
Name: Jiwon Seo
Project: CSC 452 Project 1: Graphics library
Due Date: Sunday, Febrary 6, 2022 by 11:59 pm/ Spring 2022/
Description:
  This project is writing a small graphics library that can set a pixel to a
  particular color, draw some basic shapes and read keypreses.
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <asm-generic/ioctls.h>
#include <termios.h>
#include "iso_font.h"

#define RGB(r,g,b) (unsigned short) r<<11+(g<<6) +(b))
typedef unsigned short color_t;
typedef unsigned short uint16_t;

// global variables declared
int fd;
struct fb_fix_screeninfo finfo;
struct fb_var_screeninfo vinfo;
color_t * addr;
/**
init_graphics() functions does 4 tasks:

1. open the graphics device-framebuffer. /dev/ file system, open() syscall
2. memory share by mmap()
      void *mmap(void *addr, size_t length, int prot, int flags,
                       int fd, off_t offset);
3. ioctl
4. TCGETS and TCSETS for disabling keypress echo

System call(s) allowed
: open, ioctl, mmap
*/
void init_graphics()
{
  fd = open("/dev/fb0", O_RDWR);
  ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
  ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
  short red=0xFF;
  short green=0xFF;
  short blue=0xFF;
  //5 upper bit for red, 6 middle bit for green, 5 lower bit for blue.
  color_t color = color&(red<<11)|color&(green<<6)|color &(blue); //bit shifting and masking for color
  addr= mmap(NULL,vinfo.yres_virtual*finfo.line_length, PROT_READ | PROT_WRITE,MAP_SHARED,fd,0);
  //disable cannonical mode
  struct termios orig_termios;
  ioctl(0, TCGETS,&orig_termios);
  orig_termios.c_lflag &= ~(ECHO | ICANON);
  ioctl(0, TCSETS, &orig_termios);
}
/**
exit_graphics() function does undo whatever it is that needs to be cleaned up before program exits.

Clean up the program, close() and munmap() for initialization.
ioctl to reenable key press echoing and buffering.

System call(s) allowed
: ioctl
*/
void exit_graphics(){
  ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
  ioctl(fd, FBIOGET_FSCREENINFO, &finfo);
  close(fd);
  munmap(0,vinfo.yres_virtual*finfo.line_length);
}

/**

clear_screen() function clear the STDOUT screen in QEMU.

Use ANSI escape code to clear screen rather than blanking it by drawing a giant
rectangle of black pixel.
ANSI escape codes are a sequence of characters that are not meant to eb displayed
as text but rather interpreted as commands to the terminal.

System call(s) allowed
: write
*/
void clear_screen(){
  write(STDOUT_FILENO,"\033[2J",4); //the length is 4 because "\033 is one, [ for one 2 for one, J for one"
}

/**
getkey() function get the user input by system call select and read.

Since read() is blocking and that will cause our program not be able to draw
unless user type something,
so we use select to only read when user type something.

System call(s) allowed
: select, read
*/
char getkey(){
  char buffer;
  fd_set rd;
  struct timeval tv;
  int error;
  FD_ZERO(&rd);
  FD_SET(0,&rd);
  error = select(1,&rd,NULL,NULL,&tv);
  if(error>0){
    read(0, &buffer, 1);
  }
  return buffer;
}

/**
sleep_ms() function make our program sleep between frames of graphics being drawn.

System call(s) allowed
: nanosleep
*/
void sleep_ms(long ms){
  struct timespec req={
    (int)(ms/1000), (ms%1000)*100000
  };
  nanosleep(&req, NULL);
}

/*
draw_pixel() function set the pixel at coordiante (x,y) to the specified color.
No syscall allowed. we use mapped array "addr" to change the color of the pixel.
The parameter is x, x coordiante of the pixel,
y, y coordinate of the pixel.
color, color of the pixel.
*/
void draw_pixel(int x, int y, color_t color){
  int coordinate = y * (vinfo.xres_virtual) + x;
  addr[coordinate] = color;
}

/*
draw_rect() function draw the rectangel using the draw_pixel() funciton.
The parameter is x1, x coordinate
x2, y coordinate
width, width of the rectangle,
height, height of the rectangle,
c, color of the rectange.
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){
  int x,y;
  for(x=x1; x<=x1+width; x++){
    draw_pixel(x,y1,c);
    draw_pixel(x,y1+height,c);
  }
  for(y=y1; y<=y1+height; y++){
    draw_pixel(x1,y,c);
    draw_pixel(x1+height, y,c);
  }
}
/*
draw_char() function is helper function for draw_text() function
The parameter is x, x coordinate
y, y coordinate
characters, 1 character input
c, color of character

what draw_char does is drawing the character for one character
If string is given to draw_text() function,
draw_char only draw one character from whole strings.
*/
void draw_char(int x, int y, char character, color_t c){
  int i;
  for(i=0; i<16; i++){
    int ch = iso_font[(character*16+i)];
    int j;
    for(j=0;j<8;j++){
      if( ch>>j & 1){
        draw_pixel(x+j,y+i,c);
      }
    }
  }
}
/*
draw_text() draw the string into the stdout.
The parameter is x, x coordinate
y, y coordinate
*text string input with no limitations in length
c, the color of text

What draw_text() does is writing the text into the screen.
*/
void draw_text(int x, int y, const char *text, color_t c){
  int i=0;
  while(text[i]!='\0'){ //when there is 0, stop there.
    draw_char(x,y, text[i], c); //pass the character one by one to helper func. 
    x+=8;
    i++;
  }
}
