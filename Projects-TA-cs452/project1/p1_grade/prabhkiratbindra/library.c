#include <linux/fb.h>                       // structs fb_(var/fix)_screeninfo
#include <fcntl.h>                          // open()
#include <sys/mman.h>                       // mmap(), munmap()
#include <sys/ioctl.h>                      // ioctl()
#include <termios.h>                        // struct termios
#include <sys/select.h>                     // select()
#include <unistd.h>                         // read(), close()
#include <time.h>                           // nanosleep

#include "library.h"
#include "iso_font.h"

int fbfd  = 0;                              // frame buffer file descriptor
int ttyfd = 0;                              // terminal file descriptor
color_t* fbp = NULL;                        // frame buffer pointer (mapped)
size_t mmapsize = 0;                        // total size of the mmap()'ed file

void init_graphics() {                      // open, mmap, ioctl

    if ((fbfd = open("/dev/fb0", O_RDWR)) == -1) {
        _exit(1);                           // Error: cannot open framebuffer device
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        _exit(2);                           // Error reading variable screen information
    }

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        _exit(3);                           // Error reading fixed screen information
    }

    mmapsize = vinfo.yres_virtual * finfo.line_length;

    fbp = (color_t*) mmap(NULL, mmapsize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

    if ((int) fbp == -1) {
        _exit(4);                           // Error: failed to map framebuffer device to memory
    }

    if ((ttyfd = open("/dev/tty", O_RDWR)) == -1) {
        _exit(5);                           // Error: cannot open terminal
    }

    struct termios tos;

    if (ioctl(ttyfd, TCGETS, &tos) == -1) {
        _exit(6);                           // Error reading terminal settings
    }

    tos.c_lflag &= ~(ICANON | ECHO);        // disable keypress echo & buffering

    if (ioctl(ttyfd, TCSETS, &tos) == -1) {
        _exit(7);                           // Error writing terminal settings
    }
}

/**
 * Undo changes/clean-up before program exits
*/
void exit_graphics() {                      // ioctl

    close(fbfd);
    munmap(fbp, mmapsize);

    struct termios tos;

    if (ioctl(ttyfd, TCGETS, &tos) == -1) {
        _exit(8);                           // Error reading terminal settings
    }

    tos.c_lflag |= (ICANON | ECHO);         // re-enable keypress echo & buffering

    if (ioctl(ttyfd, TCSETS, &tos) == -1) {
        _exit(9);                           // Error writing terminal settings
    }

    close(ttyfd);
}

void clear_screen() {                       // write

    if (write(ttyfd, "\033[2J", 5) == -1) {
        _exit(10);                          // Error clearing screen
    }
}

char getkey() {                             // select, read

    fd_set readfds;

    FD_ZERO(&readfds);                      // initializes the fdset to have zero bits for all fds
    FD_SET(0, &readfds);                    // sets the bit for the specified fd in the given fdset

    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 0;

    // use the Linux non-blocking system call select() to check if a fd is ready
    int retval = select(1, &readfds, NULL, NULL, &timeout);

    char buf = 0;

    // if so, read() it
    if (retval == -1) {

        _exit(11);                          // Error watching stdin for reading

    } else if (retval) {

        if (read(0, &buf, 1) == -1) {       // data is available; read 1 character
            _exit(12);                      // error reading from stdin
        }

    } else {

        // no data is available
    }

    return buf;
}

void sleep_ms(long ms) {                    // nanosleep

    struct timespec req;
    req.tv_sec  = 0;
    req.tv_nsec = ms * 1000000;

    nanosleep(&req, NULL);
}

void draw_pixel(int x, int y, color_t color) {

    // scale the base address of the memory-mapped framebuffer using pointer arithmetic
    *(fbp + y*640 + x) = color;
}

/**
 * make a rectangle with the following corners:
 *      x1, y1
 *      x1 + width, y1
 *      x1 + width, y1 + height
 *      x1, y1 + height
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {

    int i, j;

    if ((x1 + width >= 640) || (y1 + height >= 480)) {
        return;                             // do not draw an invalid rectangle
    }

    for (j = y1; j <= y1 + height; ++j) {

        for (i = x1; i <= x1 + width; ++i) {

            draw_pixel(i, j, c);
        }
    }
}

void draw_char(int x, int y, const char letter, color_t c) {

    int i, j, n;

    for (i = letter * 16, j = 0; i < (letter + 1) * 16; ++i, ++j) {

        char byte = iso_font[i];

        char mask = 0x01;                   // 00000001

        for (n = 0; n < 8; ++n) {

            if ((mask & byte) == (0x01 << n)) {
                draw_pixel(x + n, y + j, c);
            }

            mask = mask << 1;
        }
    }
}

void draw_text(int x, int y, const char* text, color_t c) {

    while (*text) {                         // until '\0' is found, ...
        draw_char(x, y, *text, c);
        x += 8;
        text++;
    }
}
