// The library file for our projects.
// Written by Ryan Alterman
// Revised on 2/5/2022

#include "graphics.h"
#include "iso_font.h"

// Includes used for various types and such
#include <linux/fb.h>
#include <termios.h>

// Includes used for system calls
#include <time.h>          // nanosleep syscall
#include <sys/select.h>    // select syscall
#include <unistd.h>        // read/write syscall
#include <fcntl.h>         // open syscall
#include <sys/mman.h>      // mmap syscall
#include <sys/ioctl.h>     // ioctl syscall

// Global variables
int fb;
color_t* mem;
int len;
int yres;
int line_len;

void init_graphics()
{
    fb = open("/dev/fb0", O_RDWR);

    struct fb_var_screeninfo varScreen;
    struct fb_fix_screeninfo fixScreen;

    ioctl(fb, FBIOGET_VSCREENINFO, &varScreen);
    ioctl(fb, FBIOGET_FSCREENINFO, &fixScreen);

    yres = varScreen.yres_virtual;
    line_len = fixScreen.line_length;
    len = yres * line_len;

    mem = (color_t*)mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, 
    fb, 
    0);

    // Set these attributes for stdin
    struct termios term;
    ioctl(0, TCGETS, &term);
    term.c_lflag &= ~(ECHO | ICANON);
    ioctl(0, TCSETS, &term);
}

void exit_graphics()
{
    // Free resources used
    munmap(mem, 0);
    close(fb);

    // Revert the terminal settings
    struct termios term;
    ioctl(0, TCGETS, &term);
    term.c_lflag |= ECHO;
    term.c_lflag |= ICANON;
    ioctl(0, TCSETS, &term);
}

void clear_screen()
{
    // Tell the terminal to clear the screen
    write(0, "\033[2J", 4);
}

char getkey()
{
    char key = ' ';

    fd_set rfds;
    struct timeval t;
    int err;

    FD_ZERO(&rfds);
    // Watch stdin for input
    FD_SET(0, &rfds);

    // Set the timeout
    t.tv_sec = 0;
    t.tv_usec = 0;

    // Detect if any input occured
    err = select(1, &rfds, NULL, NULL, &t);

    // err 0 is timeout, where err -1 is failure
    if(err != 0 && err != -1)
    {
       read(0, &key, 1);
    }

    return key;
}

void sleep_ms(long ms)
{
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = (ms * (1000000L));

    nanosleep(&t, NULL);
}

void draw_pixel(int x, int y, color_t color)
{
    int pixelPos = x + y * (line_len / 2);
    *((color_t*)(mem + pixelPos)) = color;
}

void draw_rect(int x1, int y1, int width, int height, color_t c)
{
    int x;
    int y;
    for(y = 0; y < height; y++)
    {
        for(x = 0; x < width; x++)
        {
            draw_pixel(x + x1, y + y1, c);
        }
    }
}

void draw_text(int x, int y, const char* text, color_t c)
{
    const char* curChar = text;
    int curLetter = 0;
    int xOffset = 8;
    while(*curChar != '\n')
    {
        int i;
        int j;
        // Row
        for(i = 0; i < 16; i++)
        {
            char curRow = iso_font[*curChar * 16 + i];
            // Get each bit of each column
            for(j = 0; j < 8; j++)
            {
                int bit = (curRow & (1 << j)) >> j;
                // Draw the bit if the bit is to be shown
                if(bit == 1)
                {
                    int xPos = x + j + (curLetter * xOffset);
                    int yPos = y + i;
                    draw_pixel(xPos, yPos, c);
                }
            }
        }

        curChar++;
        curLetter++;
    }
}

color_t convertRGB(color_t R, color_t G, color_t B)
{
    // Check and clamp all inputs to be between 0 and 255
    if(R >= 256)
    	R = 255;
    if(R < 0)
    	R = 0;

    if(G >= 256)
    	G = 255;
    if(G < 0)
    	G = 0;

    if(B >= 256)
    	B = 256;
    if(B < 0)
    	B = 0;

    color_t red = (R << 11) & 0xF800;
    color_t green = (G << 5) & 0x07E0;
    color_t blue = B & 0x001F;
    color_t color = red | green | blue;

    return color;
}
