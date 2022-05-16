/**
 * File: library.c
 * Author: Tam Duong 
 * 
 * Purpose: create the API for drawing on the terminal with code. All
 * this make use of system calls
 * 
 */
// the provided library
#include "library.h"
#include "iso_font.h"

//library from the systems
#include <sys/select.h> 
#include <unistd.h>
#include <linux/fb.h> 
#include <termios.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h> 
#include <time.h>
#include <stdlib.h>

color_t *buffer; 
int length, depth;
struct termios ori, cur;
struct fb_fix_screeninfo bitDep;
struct fb_var_screeninfo virRes;
int fileDesc; 

/**
 * init_graphics() will initialize the graphic API by mapping 
 * memory space with frameBuffer, and disable key echoing
 */
void init_graphics() {
    // initialize the mapping of memory space to frame buffer
    fileDesc = open("/dev/fb0", O_RDWR);
    ioctl(fileDesc, FBIOGET_FSCREENINFO, &bitDep);
    depth = bitDep.line_length;
    ioctl(fileDesc, FBIOGET_VSCREENINFO, &virRes);  
    length = virRes.yres_virtual; 
    buffer = (color_t*) mmap(NULL, length*depth, PROT_READ|PROT_WRITE, MAP_SHARED, fileDesc, 0);  
 
    //disable key echoing to make sure text does not get in screen that we don't want
    ioctl(STDIN_FILENO, TCGETS, &cur);
    ioctl(STDIN_FILENO, TCGETS, &ori);                           
    cur.c_lflag &= ~(ICANON | ECHO);
    ioctl(STDIN_FILENO, TCSETS, &cur); 
}

/**
 * exit_graphics() will clear unmap framebuffer memory, enable 
 * key echoing again we are done with the graphics. 
 */
void exit_graphics() {
     //reset the key echo
    ioctl(STDIN_FILENO, TCSETS, &ori);
    // unmap framebuffer
    munmap(buffer, length*depth);
    //close file
    close(fileDesc);
}

/**
 * clear_screen() will clear the screen
 */
void clear_screen() {
    write(STDOUT_FILENO, "\033[2J", sizeof("\033[2J"));
}

/**
 * getKey() will get the character from the user input and return
 */
char getkey() {
    char key;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds); 
    select(STDOUT_FILENO, &rfds, NULL, NULL, NULL);
    read(STDIN_FILENO, &key, 1);
    return key;
}

/**
 * sleep_ms() will let program pause for a number of ms
 */
void sleep_ms(long ms) {
    struct timespec timer;
    timer.tv_sec = 0;
    timer.tv_nsec = ms * 1000000;
    nanosleep(&timer, NULL);
}

/**
 * draw_pixel() will draw the pixel at the (x, y) coord
 * on the screen
 */
void draw_pixel(int x, int y, color_t c) { 
    if (x < 0 ||x >= depth/2 || y < 0|| y >= length) {
        return;
    }
    buffer[y*(depth/2)+x] = c; 
}

/**
 * draw_rect() will draw a rectangle at x1, y1 with width and height
 * and the specified color c
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    int dx, dy;
    for (dx = 0; dx <= width; dx++) {
        draw_pixel(x1+dx, y1, c);
        draw_pixel(x1+dx, y1+height, c);
    }
    for (dy = 0; dy <= height; dy++) {
        draw_pixel(x1, y1+dy, c);
        draw_pixel(x1+width, y1+dy, c);
    }
}

/**
 * draw_char() will draw a character at (x, y) coord
 * with c color 
 */
void draw_char(int x, int y, char chara, color_t c) {
    int line, bit;
    for (line = 0; line < 16; line++) {
        char curLine = iso_font[chara*16+line];
        for (bit = 0; bit < 8; bit++) {
            int cur = (curLine >> (7-bit)) & 0x1;
            if (cur) {
                draw_pixel(x+7-bit, y+line, c);
            } 
        }
    }
}

/**
 * draw_text(x, y, text, c) will draw the specified text
 * starting from coordinate x, y with color c
 */
void draw_text(int x, int y, const char *text, color_t c) {
    int dx = 0;
    int cur;
    for (cur = 0; text[cur] != '\0'; cur++) {
        draw_char(x+dx, y, text[cur], c);
        //increase 8 since it is the width of character
        dx+=8;
    }
}

