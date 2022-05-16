/*
 * File: library.c
 * Author: Xinyi Shao
 */

#include "graphics.h"
#include "iso_font.h"

#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <termios.h>
#include <time.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

int fd, area, row, col;
void *addr;
struct termios term;

/**
 * This function is to initialize the graphics library
 */
void init_graphics() {
    // Open the device file that represents 0th framebuffer attached to the computer
    // O_RDWR: it can be read and written
    fd = open("/dev/fb0", O_RDWR);
    if (fd == -1) return;

    // Pass it a file descriptor and a particular number that represents the request you’re making of that device,
    // plus a pointer to where the result will go (if applicable)
    struct fb_var_screeninfo fb_var;
    struct fb_fix_screeninfo fb_fix;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &fb_var) == -1) return;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_fix) == -1) return;

    // virtual resolution * bit depth
    area = fb_var.yres_virtual * fb_fix.line_length;

    col = fb_fix.line_length / 2;
    row = fb_var.yres_virtual;

    // Use mmap to map a file into our address space so that we can treat it as an array and use loads and stores
    // to manipulate the file contents
    // PROT_WRITE: pages may be written
    // MAP_SHARED: let other parts of the OS use the framebuffer
    addr = mmap(NULL, area, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (ioctl(STDIN_FILENO, TCGETS, &term) == -1) return;
    term.c_lflag ^= ICANON;
    term.c_lflag ^= ECHO;
    if (ioctl(STDIN_FILENO, TCSETS, &term) == -1) return;

    // blankout the screen
    clear_screen();
}

/**
 * This function is to undo whatever it is that needs to be cleaned up before the program exits
 */
void exit_graphics() {
    // Reenable key press echoing and buffering
    if (ioctl(STDIN_FILENO, TCGETS, &term) == -1) return;
    term.c_lflag |= ICANON;
    term.c_lflag |= ECHO;
    if (ioctl(STDIN_FILENO, TCSETS, &term) == -1) return;

    // Unmap the page of memory used as framebuffer
    if (munmap(addr, area) == -1) return;

    // Close the file to describe framebuffer devices
    if (close(fd) == -1) return;
}

/**
 * This function is to blankout the screen
 */
void clear_screen() {
    write(STDOUT_FILENO, "\033[2J", 4);
}

/**
 * This function is to check if there is keypress, if so, read it
 * @return char key
 */
char getkey() {
    // Add stdin (0) to the file descriptor
    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(STDIN_FILENO, &rd);

    char key = '\0';
    int keyPressed = select(STDIN_FILENO + 1, &rd, NULL, NULL, NULL);

    if (keyPressed > 0) {
        read(STDIN_FILENO, &key, 1);
    }

    return key;
}

/**
 * This function is to make our program sleep between frames of graphics being drawn
 * @param ms milliseconds to sleep
 */
void sleep_ms(long ms) {
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&req, NULL);
}

/**
 * This function will draw pixel onto screen, the image will be stored in row-major order
 * @param x coordinates
 * @param y coordinates
 * @param color RGB
 */
void draw_pixel(int x, int y, color_t color) {
    // Boundary checking
    if (x < 0 || x > col || y < 0 || y > row) return;

    int offset = (y * col) + x;
    *((color_t *) (addr) + offset) = color;
}

/**
 * This function will draw rectangle with corners (x, y), (x+width, y), (x+width, y+height), (x, y+height)
 * @param x coordinates
 * @param y coordinates
 * @param width
 * @param height
 * @param color RGB
 */
void draw_rect(int x, int y, int width, int height, color_t color) {
    // Boundary checking
    if (x < 0 || x > col || x + width > col || y < 0 || y > row || y + height > row) return;

    int i, j;
    int offset = (y * col) + x;
    for (i = 0; i <= height; i++) {
        for (j = 0; j <= width; j++) {
            if (i == 0 || i == height || j == 0 || j == width) {
                *((color_t *) (addr) + offset + j + (i * col)) = color;
            }
        }
    }
}

/**
 * This helper function is to draw a single character based on iso_font.h
 * @param x coordinates
 * @param y coordinates
 * @param letter
 * @param color RGB
 */
void draw_char(int x, int y, const char letter, color_t color) {
    // Boundary checking
    // Each letter is 8x16 pixels
    if (x < 0 || x > col || x + 8 > col || y < 0 || y > row || y + 16 > row) return;

    int i, j;
    int offset = (y * col) + x;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 8; j++) {
            if (((iso_font[(int) letter * 16 + i] >> j) & 0x01) == 1) {
                *((color_t *) (addr) + offset + j + (i * col)) = color;
            }
        }
    }
}

/**
 * This function is to draw the string with the specified color at the starting location (x,y)
 * @param x coordinates
 * @param y coordinates
 * @param text string to be drawn
 * @param color RGB
 */
void draw_text(int x, int y, const char *text, color_t color) {
    // Boundary checking
    if (x < 0 || x > col || y < 0 || y > row) return;
    
    int i;
    for (i = 0; *text != '\0'; text++, i++) {
        draw_char(x + i * 8, y, *text, color);
    }
}

