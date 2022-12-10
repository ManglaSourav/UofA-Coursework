/*
 * Author: Parker Jones
 *
 * Defines the functions for the library. This implements the graphics functions
 * and stores the information needed to use the frame buffer.
 */

#include "library.h"
#include "iso_font.h"

#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/fb.h>
#include <termios.h>
#include <unistd.h>

// File descriptor for the frame buffer
static int graphics = -1;

// Local memory-mapped screen file
static color_t *screen = NULL;
// Screen information
static size_t screen_size = 0;
static unsigned int screen_width = 0, screen_height = 0;

#define PIXEL(x, y) ((y) * screen_width + (x))

// Original terminal settings
static tcflag_t terminal_settings = 0;

/*
 * Open the frame buffer and map it to local memory. Disable terminal echo and
 * canonical mode.
 * Note that this function gets everything ready for graphics but does not
 * modify the graphics upon initialization, i.e. the screen is not auto-cleared.
 */
void init_graphics()
{
    // Open the frame buffer
    graphics = open("/dev/fb0", O_RDWR);

    // Find the size in bytes of the screen
    struct fb_var_screeninfo screen_info;
    ioctl(graphics, FBIOGET_VSCREENINFO, &screen_info);
    screen_width = screen_info.xres_virtual;
    screen_height = screen_info.yres_virtual;
    screen_size = screen_width * screen_height * (screen_info.bits_per_pixel / 8);

    // Map the screen file to local memory
    screen = mmap(NULL, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, graphics, 0);

    // Disable terminal echo and canonical mode
    struct termios settings;
    ioctl(0, TCGETS, &settings);
    terminal_settings = settings.c_lflag;
    settings.c_lflag &= ~(ECHO | ICANON);
    ioctl(0, TCSETS, &settings);
}

/*
 * Close frame buffer, unmap memory, and restore settings.
 * Note that this cleans up the resources that were used to interface with the
 * frame buffer, but does not do extra work, such as clearing the screen.
 */
void exit_graphics()
{
    // Close frame buffer
    close(graphics);
    // Unmap screen memory
    munmap(screen, screen_size);

    // Restore modified terminal settings
    struct termios settings;
    ioctl(0, TCGETS, &settings);
    settings.c_lflag = terminal_settings;
    ioctl(0, TCSETS, &settings);
}

/*
 * Blank the screen
 */
void clear_screen()
{
    write(1, "\033[2J", 4);
}

/*
 * Read a key from the input. If there is no input, 0 is returned.
 */
char getkey()
{
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(0, &read_set);

    // Zero time will make select return immediately
    struct timeval time = {0, 0};

    if (select(1, &read_set, NULL, NULL, &time))
    {
        char result;
        read(0, &result, 1);
        return result;
    }
    return 0;
}

// This is supposed to be in time.h, but it's not on QEMU.
extern int nanosleep(const struct timespec*, struct timespec*);

/*
 * Sleep for the specified milliseconds.
 */
void sleep_ms(long ms)
{
    struct timespec time = {ms / 1000, (ms % 1000) * 1000000};
    nanosleep(&time, NULL);
}

/*
 * Set one particular pixel on the screen.
 * This function performs bounds checking.
 */
void draw_pixel(int x, int y, color_t color)
{
    if (x < 0 || y < 0 || x >= screen_width || y >= screen_height) return;
    screen[PIXEL(x, y)] = color;
}

/*
 * Create a rectangle outline with the given size.
 * x1 and y1 are the top left corner of the rect.
 * Note that this rect function behaves different from the usual implementation
 * of drawing the entire shape.
 * It instead behaves like a draw_rect_outline function.
 */
void draw_rect(int x1, int y1, int width, int height, color_t c)
{
    for (int i = 0; i < width; i++)
    {
        draw_pixel(x1 + i, y1, c);
    }
    for (int i = 0; i < width; i++)
    {
        draw_pixel(x1 + i, y1 + height - 1, c);
    }
    for (int i = 0; i < height; i++)
    {
        draw_pixel(x1, y1 + i, c);
    }
    for (int i = 0; i < height; i++)
    {
        draw_pixel(x1 + width - 1, y1 + i, c);
    }

}

/*
 * Draw one character to the screen.
 */
static void draw_char(int x, int y, char c, color_t color)
{
    int index = c * ISO_CHAR_HEIGHT;
    for (int row = 0; row < 16; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            if ((iso_font[index + row] >> col) & 1)
            {
                draw_pixel(x + col, y + row, color);
            }
        }
    }
}

/*
 * Draw a line of text to the screen. This does not handle newlines.
 */
void draw_text(int x, int y, const char *text, color_t c)
{
    char current;
    while ((current = *text))
    {
        draw_char(x, y, current, c);
        x += 8;
        text++;
        if (x >= screen_width) break;
    }
}
