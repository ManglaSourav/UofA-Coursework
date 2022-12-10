/*
* File:    library.c
* Date:    February 1, 2022
* Author:  Fernando Ruiz 
* Purpose: The following program is a small graphics library that can set a pixel 
* to a particular color, draw a rectangle, read keypresses, and draw text. All 
*  functions are implemented using only linux system calls. 
*/

#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include "iso_font.h"
#include "graphics.h"
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

//globals used for clean-up
struct fb_var_screeninfo screen_info;
struct fb_fix_screeninfo fixed_info;
color_t *buffer;
size_t size;
int fd;

/*
* init_graphics() -- The following function initiates
* graphics using the linux syscalls open(), ioctl(), mmap(). The fcn
* keypress_switch() is called to disable to keys. 
*/
void init_graphics() {
    //open fd, then error check.
    fd = open("/dev/fb0", O_RDWR);
    if (fd == -1) {
        _exit(1);
    }

    //get screen info while error checking.
    if (ioctl(fd, FBIOGET_VSCREENINFO, &screen_info) == -1 || 
        ioctl(fd, FBIOGET_FSCREENINFO, &fixed_info)  == -1) {
        _exit(1);
    }

    //assign size using screen in mult. 
    size = (screen_info.yres_virtual * fixed_info.line_length);
    //set up buffer using mmap with fd and size
    buffer = mmap(0, size, PROT_WRITE, MAP_SHARED, fd, 0);
    //mmap error check
    if (buffer == MAP_FAILED){
        _exit(1);
    }

    //disable keys
    keypress_switch(0);
}

/*
* exit_graphics() -- The following function exits
* graphics using the linux syscall ioctl() in the fcn
* keypress_switch() which is called to renable key presses.
* The buffer is unmapped and the opened file is closed.
*/
void exit_graphics() {
    //re-enable keys
    keypress_switch(1);

    //clean up mmap
    if (buffer && buffer != MAP_FAILED) {
        munmap(buffer, size);
    }

    //clean up fd
    if (fd >= 0) {
        close(fd);
    }
}

/*
* keypress_switch(int button) -- The following function
* disables and enables keypress using the syscalls
* open() and ioctl().
*/
void keypress_switch(int button){
    struct termios term;

    int curr_term = open("/dev/tty", O_RDWR);
    if (curr_term == -1) {
       _exit(1);
    }

    if (ioctl(curr_term, TCGETS, &term) == -1) {
        _exit(1);
    }

    //using if statements to make the fcn reusable. 
    if (button == 0) {
        //disable keys
        term.c_lflag &= ~ICANON;
        term.c_lflag &= ~ECHO;
        //term.c_lflag &= ~(ECHO|ICANON);
    } else if (button == 1) {
        //enable keys
        term.c_lflag |= ICANON;
        term.c_lflag |= ECHO;
        //term.c_lflag |= (ECHO|ICANON);
    }

    if (ioctl(curr_term, TCSETS, &term) == -1) {
       _exit(1);
    }

    close(curr_term);
}

/*
* clear_screen() -- The following function clears
* the console of all input and output using the 
* syscall write() with the ANSI escape code. 
*/
void clear_screen() {
    //ANSI escape code
    char code[] = "\033[2J";
    int  strLen = sizeof(code) - 1;
    // write to clear
    write(STDOUT_FILENO, code, strLen);
}

/*
* sleep_ms() -- The following function sleeps
* for the passed in amount of milliseconds using
* the syscall nanosleep(). 
*/
void sleep_ms(long ms) {
    struct timespec req;
    req.tv_sec  = 0;
    // muli. by number stated in spec.
    req.tv_nsec = (ms * 1000000);

    int check = nanosleep(&req, NULL);
    if (check == -1){
       _exit(1);
    }
}

/*
* draw_pixel(int x, int y, color_t color) -- The following 
* function sleeps draws a pixel on the screen by scaling
* coordinates and setting to a color using the bass buffer address. 
*/
void draw_pixel(int x, int y, color_t color) {
    if (y <= 479 && x <= 639 && x >= 0 && y >= 0){
      unsigned long scale = ((fixed_info.line_length / (16/8)) * y) + x;
      *(buffer + scale) = color;
    }
}

/*
* draw_rect(int x1, int y1, int width, int height, color_t c) --
* The following function uses the function draw_pixel() to create
* a rectangle with the parameters for its location and dimension.  
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    int x, y;
    for (x = x1; x < (x1 + width); x++){
        for (y = y1; y < (y1 + height); y++) {
          draw_pixel(x, y, c);
        }
    }
}

/*
* draw_pixel(int x, int y, color_t color) -- The following 
* function sleeps draws a pixel on the screen by scaling
* coordinates and setting to a color using the bass buffer address. 
*/
char getkey() {
    struct timeval tv;
    char keypress;
    fd_set rfds;
    

    //clears set (rfds)
    FD_ZERO(&rfds);
    //set stdin to rfds (STDIN)
    FD_SET(STDIN_FILENO, &rfds);

    //instant
    tv.tv_sec  = 0;
    tv.tv_usec = 0;

    int check = select(1, &rfds, NULL, NULL, &tv);

    if(check == 1){
      read(STDIN_FILENO, &keypress, 1);
      return keypress;
    } else {
      return 0;
    }
}

/*
* draw_text(int x, int y, const char *text, color_t c) -- The following 
* function draws text using the included apple font. It uses bit shift 
* and masking to parse the data and calls the function draw_pixel() display
* each pixel of the text. 
*/
void draw_text(int x, int y, const char *text, color_t c) {
    int constX = x;
    int constY = y;
    int i, j, k; 
    for (i = 0; text[i]!='\0'; i++) {
        int letter  = text[i];
        //parses through hex for ascii bottom-up
        int fontIdx = (letter * 16) + 16; 
        for (j = 0; j < 16; j++) {
            int hex = iso_font[fontIdx-j];
            for (k = 0; k <= 7; k++) {
              if ((hex >> k) & 0x01) {
                // bit is equal to 1
                draw_pixel(x, y, c);  
              }
              //next pixel in row
              x = x + 1;
            }
            //reset x to begining 
            x = constX;
            //next row
            y = y - 1;
        }
        //set up for next letter
        x = x + 8;
        constX = x;
        y = constY;
    }
}

/*
* pixel_color(unsigned short red, unsigned short green, unsigned short blue) --
* The following function converts a rgb value to a 16 bit value to be used in 
* coloring the pixel using masking and bit shifting.
*/
color_t pixel_color(unsigned short red, unsigned short green, unsigned short blue) {
  color_t pixel_color  = ((red   & 0x1f) << 11);
  pixel_color         |= ((green & 0x3f) <<  5);
  pixel_color         |= ((blue  & 0x1f) <<  0);

  return pixel_color;
}