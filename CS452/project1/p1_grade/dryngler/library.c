/**
 * @file driver.c
 * @author Daniel Ryngler
 * @brief Example program to show graphics library is performing properly
 * @version 0.1
 * @date 2022-02-06
 * 
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h> 
#include <termios.h>
#include <time.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <unistd.h>
#include "iso_font.h"


// global variables needed for the graphics library
char *fbAddress;
int lineLen;
int mmapFileSize;
struct termios termio;
typedef unsigned short color_t; 
char buf[1];


/**
 * @brief Work necessary to initialize the graphics library. 
 * This includes accessing the frame buffer, getting information of the screen being drawn on,
 * mmapping the frame buffer file, and disabling keyboard echo while drawing. 
 * 
 */
void init_graphics(){
    //1. framebuffer
    int framebufferFd = open("/dev/fb0", O_RDWR);

    //3. get information about screen
    struct fb_var_screeninfo varScreenInfo;
    struct fb_fix_screeninfo fixScreenInfo;

    ioctl(framebufferFd, FBIOGET_VSCREENINFO, &varScreenInfo);
    ioctl(framebufferFd, FBIOGET_FSCREENINFO, &fixScreenInfo);
    lineLen = fixScreenInfo.line_length;
    mmapFileSize = varScreenInfo.yres_virtual *  fixScreenInfo.line_length;

    //2. memory mapping
    fbAddress = mmap(NULL, mmapFileSize, PROT_READ | PROT_WRITE,  MAP_SHARED, framebufferFd, 0);

    //4. Disable terminal echo
    ioctl(0, TCGETS, &termio);
    struct termios newTermio;
    newTermio = termio;
    newTermio.c_lflag &= ~ICANON;
    newTermio.c_lflag &= ~ECHO;
    ioctl(0, TCSETS, &newTermio);

    return;
}

/**
 * @brief Reset terminal settings to before init_graphics()
 * Re-enable keypress echo and buffering 
 */
void exit_graphics(){
    ioctl(0, TCSETS, &termio);
}

/**
 * @brief Use ANSI escape code to clear screen
 */
void clear_screen(){
    write(1, "\033[2J", 7);
}

/**
 * @brief Read user input via key press
 * Uses read() and select() system calls.
 * 
 * @return char, the key read from the user.
 * key defaults to 'c' is no input is read. 
 */
char getkey(){
    fd_set rfds;
    struct timeval tv;
    tv.tv_sec = 0; 
    tv.tv_usec = 1000000;

    FD_ZERO(&rfds); //watch standard in
    FD_SET(0, &rfds); 
    int retval = select(1, &rfds, NULL, NULL, &tv);

    if (retval == -1)
        return 1;
    else if (retval){
        read(0, buf, sizeof(char));
    } else {
        buf[0] = 'c';
    }

    return buf[0];
}

/**
 * @brief Use the system call nanosleep() to make program sleep between frames of graphics being drawn
 * 
 * @param ms, milliseconds to sleep for
 */
void sleep_ms(long ms){ 
    struct timespec tv;
    tv.tv_sec = 0;
    tv.tv_nsec = ms * 1000000;
    nanosleep(&tv, NULL);
}

/**
 * @brief Draw a pixel at x, y, in the memoray mapped file with given color.
 * Makes sure to only draw coordinates that are within bounds
 * 
 * @param x, x coordinate 
 * @param y, y coordinate
 * @param color, color to draw pixel
 */
void draw_pixel(int x, int y, color_t color){
    int location = x * lineLen + y*2;
    if (location >= 0 && location < mmapFileSize) {
        char *pixel = (fbAddress + location);
        *pixel = color;
    }
}

/**
 * @brief Draw a rectangle on the screen at x1, y1, with given width, height, and color.
 * Utilizes the draw_pixel() function.
 * 
 * @param x1, x coordinate 
 * @param y1, y coordinate
 * @param width, width of rectangle
 * @param height, height of rectangle
 * @param color, color to draw pixel 
 */
void draw_rect(int x1, int y1, int width, int height, color_t color){
    int i;
    for (i = 0; i < width; i++){
        draw_pixel(x1 + i, y1, color);
        draw_pixel(x1 + i, y1 + height, color);
    }
    for (i = 0; i < height; i++){
        draw_pixel(x1, y1 + i, color);
        draw_pixel(x1 + width, y1 + i, color);
    }
}

/**
 * @brief Draw a character on the screen in "iso font" by indexing the iso font array 
 * to find dimensions of char and then calling draw_pixel().
 * 
 * @param x1, x coordinate 
 * @param y1, y coordinate
 * @param chr, char to draw on screen
 * @param color, color of character
 */
void draw_character(int x, int y, const char chr, color_t color){
    int i;
    for (i = 0; i < 16; i++){
        unsigned b;
        int count = 0;
        for (b = 1 << 7; b > 0; b = b / 2){
            if (iso_font[chr*16 + i] & b){
                draw_pixel(x + i, y - count, color);
        }
            count++;
        } 
    }
}

/**
 * @brief Draw text on the screen by utlizing the draw_character() function for each char in the string.
 * 
 * @param x1, x coordinate 
 * @param y1, y coordinate
 * @param text, text to draw 
 * @param color, color of character
 */
void draw_text(int x, int y, char *text, color_t color){
	int i = 0;
	int charWidth = 8;
	char *ptr;
	for (ptr = text; *ptr != '\0'; ptr++){
		draw_character(x, y + (i * charWidth), *ptr, color);
		i++;
	}
}


