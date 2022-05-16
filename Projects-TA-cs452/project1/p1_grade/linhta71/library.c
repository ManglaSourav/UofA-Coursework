/**
 * @file library.c
 * @author Linh Ta
 * @brief 
 * 
 * @purpose This file implements all the interface defined in library.h
 * in order to implements the API for drawing things on the terminal
 * using system calls
 * 
 */

#include "library.h"
#include "iso_font.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h> 
#include <linux/fb.h> 
#include <termios.h>
#include <time.h>
#include <stdlib.h>
#include <sys/select.h> 
#include <unistd.h>

color_t *frameBuf;  
int fileDescriptor;
int yLen, depth, size;
struct termios old, new;
struct termios cur;

/**
 * init_graphics() will initialize and map to the framebuffer
 * of the file to the pointer to manipulate the display. It will
 * also obtain the information regarding the terminal screen 
 * spec and suppress the keypress echo
 */
void init_graphics() {
    //step 1: grab the first frame buffer
    fileDescriptor = open("/dev/fb0", O_RDWR);

    // step 3: get information about the screen
    struct fb_var_screeninfo virtualResolution; 
    struct fb_fix_screeninfo bitDepth;  
    ioctl(fileDescriptor, FBIOGET_VSCREENINFO, &virtualResolution);  
    ioctl(fileDescriptor, FBIOGET_FSCREENINFO, &bitDepth);
    yLen = virtualResolution.yres_virtual; 
    depth = bitDepth.line_length;
    size = yLen*depth; 

     //step 2: create the frame buffer memory space using mmap
    frameBuf = (color_t*) mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fileDescriptor, 0);  
 
    //step 4: disable keypress echo
    ioctl(STDIN_FILENO, TCGETS, &new); 
    old = new;                              
    new.c_lflag &= ~(ICANON | ECHO);
    ioctl(STDIN_FILENO, TCSETS, &new); 
}

/**
 * exit_graphics() will clear the screen and and unmap to the framebuffer
 * and set the old terminal setting so that the key is no longer suppressed
 */
void exit_graphics() {
    clear_screen(frameBuf);
    munmap(frameBuf, yLen*depth); 
    ioctl(STDIN_FILENO, TCSETS, &old);
    close(fileDescriptor);
}

/**
 * clear_screen() will clear the terminal screen
 */
void clear_screen() {
    write(STDOUT_FILENO, "\033[2J", sizeof("\033[2J"));
}

/**
 * getKey() will get the character from the key press and return 
 * that character
 * 
 * @return char 
 */
char getkey() {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds); 
    select(STDOUT_FILENO, &rfds, NULL, NULL, NULL);
    char buff;
    read(STDIN_FILENO, &buff, 1);
    return buff;
}

/**
 * sleep_ms() will sleep the programe for 
 * specified ms
 * 
 * @param ms, how long it will sleep
 */
void sleep_ms(long ms) {
    struct timespec timeStruct;
    timeStruct.tv_sec = 0;
    timeStruct.tv_nsec = ms * 1000000L;
    nanosleep(&timeStruct, NULL);
}

/**
 * draw_pixel(x, y, color) will draw the pixel at the x and y
 * coordinate on the screen with the specified color
 */
void draw_pixel(int x, int y, color_t color) { 
    if (x < 0 ||x >= depth/2) {
        return;
    }
    if (y < 0|| y >= yLen) {
        return ;
    }
    frameBuf[y*(depth/2)+x] = color; 
}

/**
 * draw_rect(x1, y1, width, heightt, c) will draw a rectangle with the 
 * top left at x1, y1 and with the width and height specified, with the
 * specified color 
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    int dx = 0, dy = 0;
    for (; dx <= width; dx++) {
        draw_pixel(x1+dx, y1+dy, c);
    }
    for (; dy <= height; dy++) {
        draw_pixel(x1+dx, y1+dy, c);
    }
    for (; dx >= 0; dx--) {
        draw_pixel(x1+dx, y1+dy, c);
    }
    for (; dy >= 0; dy--) {
        draw_pixel(x1+dx, y1+dy, c);
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
        dx+=8;
    }
}

/**
 * draw_char(x, y, character, c) will draw character at point
 * (x, y) on the terminal with color c 
 */
void draw_char(int x, int y, char character, color_t c) {
    int i, j;
    for (i = 0; i < 16; i++) {
        unsigned char curLine = iso_font[character*16+i];
        for (j = 7; j >= 0; j--) {
            int cur = (curLine >> j) & 0x1;
            if (cur) {
                draw_pixel(x+j, y+i, c);
            } 
        }
    }
}