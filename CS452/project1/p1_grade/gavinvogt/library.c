/*
 * File: library.c
 * Author: Gavin Vogt
 * Purpose: This program defines the graphics libary functions for
 * initializing/exiting, clearing the screen, and drawing.
 */

#include "graphics.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <linux/fb.h>
#include "iso_font.h"

static color_t *fb;
static size_t fileLength;
static unsigned int screenWidth;
static struct termios oldTerminalSettings;

/*
 * Initializes the graphics library
 */
void init_graphics() {
    // Open the graphics device
    int fbFile = open("/dev/fb0", O_RDWR);
    
    // Load the frame buffer dimensions
    struct fb_var_screeninfo virtRes;
    ioctl(fbFile, FBIOGET_VSCREENINFO, &virtRes);
    
    // Load the bits per pixel
    struct fb_fix_screeninfo bitDepth;
    ioctl(fbFile, FBIOGET_FSCREENINFO, &bitDepth);
    
    // Create memory mapping of the frame buffer
    fileLength = virtRes.yres_virtual * bitDepth.line_length;
    screenWidth = virtRes.xres_virtual;
    fb = mmap(NULL, fileLength, PROT_WRITE, MAP_SHARED, fbFile, 0);
    close(fbFile);
    
    // Disable keypress echo
    struct termios terminalSettings;
    ioctl(STDIN_FILENO, TCGETS, &terminalSettings);
    oldTerminalSettings = terminalSettings;
    terminalSettings.c_lflag &= ~(ICANON | ECHO);
    ioctl(STDIN_FILENO, TCSETS, &terminalSettings);
}

/*
 * Closes the graphics library
 */
void exit_graphics() {
    // Reenable keypress echo
    ioctl(STDIN_FILENO, TCSETS, &oldTerminalSettings);
    
    // Unmap the frame buffer
    munmap(fb, fileLength);
}

/*
 * Clears the screen
 */
void clear_screen() {
    // ANSI escape code to clear the screen
    write(STDIN_FILENO, "\033[2J", 4);
}

/*
 * Reads the user keypress if any, otherwise returns a null character
 */
char getkey() {
    // Create file descriptor set
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    
    // Create timeout of 0 to avoid blocking
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    int retVal = select(1, &set, NULL, NULL, &timeout);
    if (retVal == 1) {
        // Input is available
        char in;
        read(STDIN_FILENO, &in, 1);  // read 1-byte char
        return in;
    } else {
        // No input available
        return 0;
    }
}

/*
 * Sleeps for `ms` milliseconds
 */
void sleep_ms(long ms) {
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = 1000000 * ms;
    nanosleep(&req, NULL);
}

/*
 * Sets the (x, y) pixel to the given color
 */
void draw_pixel(int x, int y, color_t color) {
    color_t *pixel = fb + (y * screenWidth) + x;
    *pixel = color;
}

/*
 * Draws a rectangle with top-left corner at (x1, y1) using the
 * given width, height, and color
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    int row, col;
    for (row = 0; row < height; ++row) {
        for (col = 0; col < width; ++col) {
            draw_pixel(x1 + col, y1, c);
        }
        ++y1;
    }
}

/*
 * Draws the given character at (x, y) in the given color
 */
static void draw_char(int x, int y, char c, color_t color) {
    int row, col;
    unsigned char *char_font = iso_font + (c * ISO_CHAR_HEIGHT);
    for (row = 0; row < ISO_CHAR_HEIGHT; ++row) {
        // Get the byte for iso_font
        unsigned char rowPixels = *(char_font + row);
        for (col = 0; col < 8; ++col) {
            if (rowPixels & (0x1 << col)) {
                // Draw a pixel at this coordinate
                draw_pixel(x + col, y, color);
            }
        }
        ++y;
    }
}

/*
 * Draws the text at (x, y) in the given color
 */
void draw_text(int x, int y, const char *text, color_t c) {
    while (*text != '\0') {
        draw_char(x, y, *text, c);
        ++text;
        x += 8;
    }
}
