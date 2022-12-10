/*
    Assigment:  CSC452 Project 1
    Author:     Minghui Ke
    Purpose:    The library function which using the system call is
                used to work with framebuffer.
*/

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include "iso_font.h"
#include "graphics.h"

// *global varible* //

// file description for framebuffer
int fd;

// fd map to local memory
char *map;

// length of memory
long int length;

// size of each line.
long int line;

/*
    Initial the setting. Map the framebuffer to local memory.
    Close the echo.
*/
void init_graphics() {

    struct fb_var_screeninfo var_info;
    struct fb_fix_screeninfo fix_info;
    struct termios set;
    
    fd = open("/dev/fb0", O_RDWR);
    
    ioctl(fd, FBIOGET_VSCREENINFO, &var_info);
    ioctl(fd, FBIOGET_FSCREENINFO, &fix_info);

    line = fix_info.line_length;
    length = var_info.yres_virtual * line;

    map = (char *) mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0); 

    ioctl(STDIN_FILENO, TCGETS, &set);
    set.c_lflag &= ~ICANON;
    set.c_lflag &= ~ECHO;
    ioctl(STDIN_FILENO, TCSETS, &set);
}

/*
    Clear the screen
*/
void clear_screen() {
    write(STDOUT_FILENO, "\033[2J", 4);
}

/*
    Set back the echo and close resourse.
*/
void exit_graphics() {

    struct termios set;

    ioctl(STDIN_FILENO, TCGETS, &set);
    set.c_lflag |= ICANON;
    set.c_lflag |= ECHO;
    ioctl(STDIN_FILENO, TCSETS, &set);

    munmap(map, length);
    close(fd);
}

/*
    Use select and read to get the input from user
*/
char getkey() {

    int state;
    fd_set s_read;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&s_read);
    FD_SET(STDIN_FILENO, &s_read);
    state = select(STDIN_FILENO+1, &s_read, NULL, NULL, &tv);

    if (state <= 0) {
        return '\0';
    }
    else {
        char buff;
        read(STDIN_FILENO, &buff, sizeof(buff));
        return buff;
    }
}

/*
    Set sleep milions second
*/
void sleep_ms(long ms) {

    struct timespec time;

    time.tv_sec = 0;
    time.tv_nsec = ms * 1000000;
    nanosleep(&time, NULL);
}

/*
    draw the pixel with color on (x, y)
*/
void draw_pixel(int x, int y, color_t color) {

    long int location;

    if (x < 0 || y < 0 || x > 639 || y > 479) return;
    // Becasue int is 8 bit and pixel is 16 bit, multipfy by 2.
    location = x * 2 + y * line;
    *((unsigned short int*)(map + location)) = color;
}

/*
    draw rectangle begin at (x, y) with width and height.
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {

    int x; 
    int y;

    // Figure out where in memory to put the pixel
    for (y = y1; y < y1+height; y++) {
        for (x = x1; x < x1+width; x++) {
            draw_pixel(x, y, c);
        }
    }
}

/*
    Use iso_font to draw specific letter.
*/
void draw_char(int x, int y, char ch, color_t c) {
    
    int i, j;
    char byte;

    for (i = 0; i < 16; i++) {
        byte = iso_font[((int) ch) * 16 + i];
        for (j = 0; j < 8; j++) {
            if (byte & 1) {
                draw_pixel(x+j, y+i, c);
            }
            byte = byte >> 1;
        }
    }
}

/*
    Draw a text with multiple draw_char.
*/
void draw_text(int x, int y, const char *text, color_t c) {

    int i = 0;

    while (text[i] != '\0') {
        draw_char(x, y, text[i], c);
        x += 8;
        i++;
    }
}
