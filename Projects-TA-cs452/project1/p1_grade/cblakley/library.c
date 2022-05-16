/* Author: Cole Blakley
   Description: Implements drawing functions using only OS syscalls.
    Supports drawing simple text and shapes. Make sure to call
    init_graphics() before any other functions, as this sets up the
    internal state used by the other functions for drawing.
*/
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include "graphics.h"
#include "iso_font.h"

color_t make_color(unsigned short r, unsigned short g, unsigned short b)
{
    // rrrrrggggggbbbbb
    color_t result = 0;
    result |= r << 11;
    result |= g << 5;
    result |= b;
    return result;
}

// The screen buffer where graphics are drawn
int screen_buffer_fd;
// The memory-mapped array representing the screen buffer
color_t* screen_buffer;
unsigned int screen_buffer_size;
// The stream used for user input/terminal clearing
int terminal_fd;

void init_graphics()
{
    screen_buffer_fd = open("/dev/fb0", O_RDWR);
    struct fb_var_screeninfo resolution;
    struct fb_fix_screeninfo bit_depth;
    ioctl(screen_buffer_fd, FBIOGET_VSCREENINFO, &resolution);
    ioctl(screen_buffer_fd, FBIOGET_FSCREENINFO, &bit_depth);
    screen_buffer_size = resolution.yres_virtual * bit_depth.line_length;

    terminal_fd = STDIN_FILENO;

    struct termios term_settings;
    ioctl(terminal_fd, TCGETS, &term_settings);
    term_settings.c_lflag &= ~ICANON;
    term_settings.c_lflag &= ~ECHO;
    ioctl(terminal_fd, TCSETS, &term_settings);

    screen_buffer = mmap(NULL, screen_buffer_size,
                         PROT_READ | PROT_WRITE, MAP_SHARED,
                         screen_buffer_fd, 0);
}

void exit_graphics()
{
    struct termios term_settings;
    ioctl(terminal_fd, TCGETS, &term_settings);
    term_settings.c_lflag |= ICANON;
    term_settings.c_lflag |= ECHO;
    ioctl(terminal_fd, TCSETS, &term_settings);
    close(terminal_fd);

    close(screen_buffer_fd);
    munmap(screen_buffer, screen_buffer_size);
}

void clear_screen()
{
    write(terminal_fd, "\033[2J", 4);
}

char getkey()
{
    struct timeval wait_time = {0, 0};
    fd_set input;
    FD_ZERO(&input);
    FD_SET(terminal_fd, &input);

    int has_input = select(1, &input, NULL, NULL, &wait_time);
    if(has_input > 0) {
        char key;
        read(terminal_fd, &key, 1);
        return key;
    } else {
        return 0;
    }
}

void sleep_ms(long ms)
{
    struct timespec time = {0, ms*1000000};
    nanosleep(&time, NULL);
}

void draw_pixel(int x, int y, color_t color)
{
    screen_buffer[y * SCREEN_WIDTH + x] = color;
}

void draw_rect(int x1, int y1, int width, int height, color_t c)
{
    int x, y;
    for(y = y1; y < y1 + height; ++y) {
        for(x = x1; x < x1 + width; ++x) {
            draw_pixel(x, y, c);
        }
    }
}

void draw_char(int x, int y, char letter, color_t c)
{
    unsigned char* font_byte = iso_font + letter*FONT_HEIGHT;
    int font_row, font_col;
    for(font_row = 0; font_row < FONT_HEIGHT; ++font_row) {
        for(font_col = 0; font_col < FONT_WIDTH; ++font_col) {
            if((*font_byte >> font_col) & 0x1)
                draw_pixel(x + font_col, y + font_row, c);
        }
        ++font_byte;
    }
}

void draw_text(int x, int y, const char* text, color_t c)
{
    char curr_char;
    while((curr_char = *text++) != '\0') {
        draw_char(x, y, curr_char, c);
        x += FONT_WIDTH;
    }
}
