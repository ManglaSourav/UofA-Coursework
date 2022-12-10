/* File: library.c
/  Author: Flynn Gaur
/  CSC 452 Spring 20222
/  Instructor: J. Misurda
/  Project 1: Graphics Library
/  Purpose: This file contains all the functions to fetch buffer info 
/           and draw pixels using x, y coordinates
*/
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include "iso_font.h"
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>


struct fb_var_screeninfo vRes;
struct fb_fix_screeninfo bitDepth;
struct termios settings;
typedef unsigned short color_t;
color_t RGB(R,B,G) {
    return R << 11 | G <<5 | B;
}
int fptr;
color_t *screen;

void init_graphics() {
    fptr = open("/dev/fb0", O_RDWR);
    int x = 640;
    int y = 480;
    int totalSize;

    if (ioctl(fptr, FBIOGET_VSCREENINFO, &vRes) == -1) {
        perror("Error reading screen info");
        exit(1);
    }

    if (ioctl(fptr, FBIOGET_FSCREENINFO, &bitDepth) == -1) {
        perror("Error reading screen info");
        exit(1);
    }

    write(STDIN_FILENO, "\033[2J",8);

    //reset ICANON and ECHO cmd
    ioctl(STDIN_FILENO, TCGETS, &settings);
    settings.c_lflag &= ~ICANON;
    settings.c_lflag &= ~ECHO;
    ioctl(STDIN_FILENO, TCSETS, &settings);

    totalSize = vRes.yres_virtual * bitDepth.line_length;
    screen = (color_t*)mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fptr, 0);
}

void exit_graphics() {
    write(STDIN_FILENO, "\033[2J", 8);
    // Enable Keypress Echo
    ioctl(STDIN_FILENO,TCGETS,&settings);
    settings.c_lflag |= ECHO;
    settings.c_lflag |= ICANON;
    ioctl(STDIN_FILENO, TCSETS, &settings);
    // Unmap Buffer
    munmap(screen,vRes.yres_virtual * bitDepth.line_length); //total size
    close(fptr);
    return;
}

void clear_screen() {
    // \033 is escape sequence
    printf("\033[2J");
    int i;
    for (i=0; i<vRes.yres_virtual * bitDepth.line_length/2; i++) {
        screen[i] = 0;
    }
    return;
}

char getkey() {
    fd_set rfds;
    struct timeval tv;
    int retval;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    // set to zero to stop blinking
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    retval = select(1, &rfds, NULL, NULL, &tv);
    char key = '\0';
    if (retval!=-1 && retval) {
        read(STDIN_FILENO, &key, sizeof(key));
    }
    return key;
}

void sleep_ms(long ms) {
    if(ms > 0) {
        struct timespec tv;
        tv.tv_sec = ms / 1000;
        tv.tv_nsec = (ms%1000) * 1000000;
        nanosleep(&tv, NULL);
    }
  }

void draw_pixel(int x, int y, color_t color) {
    if (x<0 || y<0||x>=vRes.xres_virtual||y>=vRes.yres_virtual)
        return;
    screen[x+(y * vRes.xres_virtual)] = color;

    return;
}

void draw_rect(int x1, int y1, int width, int height, color_t c) {

    int i;

    // left to right
    for(i=0; i<width; i++) {
        draw_pixel(x1++, y1, c);
    }

    // down to up
    for(i=0; i<height; i++) {
        draw_pixel(x1, y1++, c);
    }

    // right to left
    for(i=0; i<width; i++) {
        draw_pixel(x1--, y1, c);
    }

    // up to down
    for(i=0; i<height; i++) {
        draw_pixel(x1, y1--, c);
    }
    return;
}

