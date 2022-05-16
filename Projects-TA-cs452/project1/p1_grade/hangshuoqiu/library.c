#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "iso_font.h"
#include <termios.h>

int fbfd = 0;
char *fbp = 0;
long int screensize = 0;
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
typedef unsigned short color_t;

void init_graphics() 
{
    // Open the file for reading and writing
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        return;
    }

    // Get fixed screen information
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        return;
    }

    // Get variable screen information
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        return;
    }

    // Figure out the size of the screen in bytes
    screensize = finfo.line_length * vinfo.yres_virtual;

    // Map the device to memory
    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1) {
        return;
    }

    // disable keypress echo
    struct termios old, new;
    tcgetattr(STDIN_FILENO, &old);          // get current settings
    new = old;                              // create a backup
    new.c_lflag &= ~(ICANON | ECHO);        // disable line buffering and feedback
    tcsetattr(STDIN_FILENO, TCSANOW, &new);
}

void exit_graphics()
{
    struct termios old, new;
    tcgetattr(STDIN_FILENO, &old);          // get current settings
    new = old;                              // create a backup
    new.c_lflag &= (ICANON | ECHO);        // disable line buffering and feedback
    tcsetattr(STDIN_FILENO, TCSANOW, &new);

    munmap(fbp, screensize);
    close(fbfd);
}

void clear_screen()
{
    // tell the terminal to clear itself
    if (write(1, "\033[2J", 7) == -1) {
        return;
    }
}

char getkey()
{
    char c;

    fd_set rd;
    struct timeval tv;
    int err;

    FD_ZERO(&rd);
    FD_SET(0, &rd);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    err = select(1, &rd, NULL, NULL, &tv);

    // check the return value
    if (err == -1) {
        return;
    } else if (err == 0) {
        return;
    } else {
        if (read(0, &c, 1) == -1) {
            return;
        } else {
            return c;
        }   
    }
}

void sleep_ms(long ms)
{
    struct timespec remaining, request = {0, ms * 1000000};

    int response = nanosleep(&request, &remaining);

    if (response != 0) {
        return;
    }
}

void draw_pixel(int x, int y, color_t color)
{
    *((unsigned short int*)(fbp + x + y*finfo.line_length)) = color;
}

void draw_rect(int x1, int y1, int width, int height, color_t c)
{
    int i;
    for (i=x1; i<x1+width; i++) {
        int j;
        for (j=y1; j<y1+height; j++) {
            draw_pixel(i, j, c);
        }
    }
}

void draw_text(int x, int y, const char *text, color_t c)
{   
    int i=0;
    // find the end of the string
    while (text[i] != '\0') {
        text_help(x+8*i, y, text[i], c);
        i++;
    }
}

void text_help(int x, int y, char c, color_t color)
{
    int i;
    for (i=0; i<16; i++) {
        char hex = iso_font[c*16+i];
        int j;
        for (j=0; j<8; j++) {
            if (hex & 1 == 1) {
                draw_pixel(x+j, y+i, color);
            }
            hex = hex >> 1;
        }
    }
    
}