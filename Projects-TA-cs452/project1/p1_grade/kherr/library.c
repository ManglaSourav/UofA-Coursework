/*
* File: library.c
* Author: Kaden Herr
* Date Created: Jan 2022
* Last editted: Feb 5, 2022
* Purpose: Create my own graphics library for QEMU to run. If you want to
* use this library, don't forget to #include "graphics.h" into you file.
*/

#include "iso_font.h"
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>

// write() and read()
#include <unistd.h>

// Structs
#include <linux/fb.h>
#include <termios.h>

// For exit() system call
#include <unistd.h>


/* Types */
typedef unsigned short color_t;

/* color_t encode_color(int,int,int) - The function turns red, green, and blue
 * number values into a 16-bit number. This number is returned as a type
 * called color_t. The first 5 bits of the first integer are converted into
 * the red value. The first 6 bits of the second integer are converted into
 * the green value. The first 5 bits of the third integer are converted into
 * the blue value.
 * The range of the parameters: encode_color(0-31,0-63,0-31)
 */
color_t encode_color(int r, int g, int b) {
    color_t color = 0;
    color = r << 11;
    g = g << 5;
    color = color | g;
    color = color | b;
    return color;
}

/* Global variables */
void *buf;
int fb_file;
int px_row_length;
int px_x_length;
int px_y_length;
size_t length;// = 614400;
struct termios terminal_settings;
struct termios original_terminal_settings;


/*
* void init_graphics() - Initalize graphics for use.
*/
void init_graphics() {

    // Open the file that contains the framebuffer
    char fb_pathname[] = "/dev/fb0";
    fb_file = open(fb_pathname,O_RDWR);
    // Error check that the file openned successfully.
    if(fb_file < 0) {
        char e[] = "ERROR: open() /dev/fb0 failed!\n";
        write(2,e,31);
        _exit(1);
    }


    // Get the screen size and bits per pixel
    struct fb_var_screeninfo var_screeninfo;
    struct fb_fix_screeninfo fix_screeninfo;
    if(ioctl(fb_file,FBIOGET_VSCREENINFO,&var_screeninfo) < 0) {
        char e[] = "ERROR: ioctl() FBIOGET_VSCREENINFO failed!\n";
        write(2,e,43);
        _exit(1);
    }
    if(ioctl(fb_file,FBIOGET_FSCREENINFO,&fix_screeninfo) < 0) {
        char e[] = "ERROR: ioctl() FBIOGET_FSCREENINFO failed!\n";
        write(2,e,43);
        _exit(1);
    }

    // Calculate how many pixels are in a row of screen real estate
    px_row_length = fix_screeninfo.line_length;
    // Get the x and y screen size
    px_x_length = var_screeninfo.xres_virtual;
    px_y_length = var_screeninfo.yres_virtual;
    // Calculate the screen resolution
    length = px_y_length*px_row_length;

    // mmap the frame buffer using the framebuffer file
    buf = mmap(NULL, length, PROT_READ|PROT_WRITE, MAP_SHARED, fb_file, 0);
    // Error check that the map was successful
    if(buf == MAP_FAILED) {
        char e[] = "ERROR: mmap() failed!\n";
        write(2,e,22);
        _exit(1);
    }


    // Get the struct that contains the current terminal settings
    if(ioctl(0,TCGETS,&terminal_settings) < 0) {
        char e[] = "ERROR: ioctl() TCGETS failed!\n";
        write(2,e,30);
        _exit(1);
    }
    if(ioctl(0,TCGETS,&original_terminal_settings) < 0) {
        char e[] = "ERROR: ioctl() TCGETS failed!\n";
        write(2,e,30);
        _exit(1);
    }
    // Disable ECHO so that key presses are no longer displayed on screen
    terminal_settings.c_lflag &= ~(ECHO);
    // Disable ICANON which disables the buffering of keypresses
    terminal_settings.c_lflag &= ~(ICANON);
    if(ioctl(0,TCSETS,&terminal_settings) < 0) {
        char e[] = "ERROR: ioctl() TCSETS failed!\n";
        write(2,e,30);
        _exit(1);
    }

    // The reverse for exit
    //terminal_settings.c_lflag |= ECHO;
    //terminal_settings.c_lflag |= ICANON;
}


/*
* void exit_graphics() - Clean up after using graphics.
*/
void exit_graphics(){

    // Unmap the buffer
    int status = munmap(buf,length);
    if(status == -1) {
        char e[] = "ERROR: buf did not successfully unmap\n";
        write(2,e,38);
    }

    // Close the frame buffer file
    status = close(fb_file);
    if(status == -1) {
        char e[] = "ERROR: fb_file did not close\n";
        write(2,e,29);
    }

    // Return the terminal settings to their original state
    ioctl(0,TCSETS,&original_terminal_settings);
}


/*
* void clear_screen() - Clear the screen of graphics using an ANSI escape
* code. ANSI escape codes are a sequence of characters that are not meant to
* be displayed as text but rather interpereted as commands to the terminal.
*/
void clear_screen() {
    // Create the sequence that tells the terminal to clear itself.
    char clear[] = "\033[2J";
    write(1,clear,4);
}


/*
* char getkey() - Read a key press from standard-in and return it as a
* character.
*/
char getkey() {

    char keyInput;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);

    // For polling, set timeval structure to 0 so select() returns immediately
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    // Monitor standard-in for a keypress
    int status = select(1,&rfds,NULL,NULL,&tv);

    // Error check select()
    if(status == -1) {
        char e[] = "ERROR: select() encountered an error\n";
        write(2,e,37);
        _exit(1);
    // If a keypress occurred, read the character into keyInput
    } else if (status) {
        if(read(0,&keyInput,1) < 0) {
            char e[] = "ERROR: error reading keyboard input\n";
            write(2,e,36);
            _exit(1);
        }
    }
    return keyInput;
}


/*
* void sleep_ms(long) - Sleep the thread for the given number of milliseconds.
*/
void sleep_ms(long ms) {
    
    int seconds = 0;
    // (If necessary) Break the sleep time into seconds and milliseconds.
    if(ms >= 1000) {
        seconds = ms / 1000;
        ms = ms % 1000;
    }

    // Set the sleep time
    struct timespec ts;
    ts.tv_nsec = ms * 1000000;
    ts.tv_sec = seconds;

    // Sleep the thread and error check nanosleep()
    if(nanosleep(&ts,NULL) < 0) {
        char e[] = "ERROR: nanosleep failed\n";
        write(2,e,24);
        _exit(1);
    }
}


/*
* void draw_pixel(int, int, color_t) - Draw a pixel to the screen at a given x
* and y coordinate. (0,0) is the top-left pixel of the screen.
*/
void draw_pixel(int x, int y, color_t color) {
    // Do not let x or y be negative
    if(x < 0) {
        x = 0;
    }
    if(y < 0) {
        y = 0;
    }

    // Do not let x or y be off the screen
    if(x >= px_x_length) {
        x = px_x_length - 1;
    }
    if(y >= px_y_length) {
        y = px_y_length - 1;
    }

    // Change the pixel in the buffer
    *(((color_t*)(buf + (y*px_row_length)) + x)) = color;
}


/*
* void fill_rect(int, int, int, int, color_t) - Makes a rectangle with corners
* (x1,y1), (x1+width,y1), (x1+width,y1+height), (x1, y1+height). The rectangle
* is filled.
*/
void fill_rect(int x1, int y1, int width, int height, color_t c) {
    int x;
    int y;

    // If width or height is 0 or less, then there is nothing to draw.
    if(width <= 0 || height <= 0) {
        return;
    }

    // Draw the rectangle
    for(y=y1; y<height+y1; y++) {
        for(x=x1; x<width+x1; x++) {
            draw_pixel(x,y,c);
        }
    }
}


/*
* void draw_rect(int, int, int, int, color_t) - Makes a rectangle with corners
* (x1,y1), (x1+width,y1), (x1+width,y1+height), (x1, y1+height). The rectangle
* is unfilled.
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    int x = x1;
    int y = y1;

    // If width or height is 0 or less, then there is nothing to draw.
    if(width <= 0 || height <= 0) {
        return;
    }

    // Draw the top line
    for(x=x1; x<width+x1; x++) {
        draw_pixel(x,y,c);
    }

    // Draw the two sides
    for(y=y1; y<height+y1; y++) {
        draw_pixel(x1,y,c);
        draw_pixel(x1+width,y,c);
    }

    y = y1 + height-1;
    // Draw the bottom line
    for(x=x1; x<width+x1; x++) {
        draw_pixel(x,y,c);
    }
}


/*
* void draw_char(int, int, char, color_t) - Draw a given character at the
* given x and y coordinates witht he given color. Use the iso_font library
* that contains pixel infomation for all of the symbols. The information is
* stored in an array. Each letter is 8x16 pixels. It is encoded as 16 1-byte
* integers.
*/
void draw_char(int x, int y, char letter, color_t c) {
    int row = 0;
    int col;
    int i;
    int cur_row;
    // Convert the character to its ASCII value.
    int ascii = (int) letter;
    //(ascii*16+0) to (ascii*16 + 15)
    for(i=ascii*16; i<ascii*16+15; i++) {
        // Access the iso_font array
        cur_row = iso_font[i];
        for(col=0; col<8; col++) {
            if(cur_row & 1) {
                draw_pixel(x+col,y+row,c);
            }
            cur_row = cur_row >> 1;
        }
        row++;
    }
}


/*
* void draw_text(int, int, const char, color_t) - Draw the given text,
* starting at the given x and y coordinates.
*/
void draw_text(int x, int y, const char *text, color_t c) {
    // Create a character pointer that we can increment from the given
    // constant pointer.
    char *txt_ptr = (char*) text;

    // Step through the array of characters, printing each one.
    while(*txt_ptr != '\0') {
        draw_char(x,y,*txt_ptr,c);
        // x+8 because each character is 8 px wide.
        x += 8;
        txt_ptr++;
    }
}