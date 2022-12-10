/*
    FILENAME: library.c
    AUTHOR: Domenic Telles
    COURSE: CSC 452
    DESCRIPTION:
        This program is a small graphics library that can set pixel to
        a particular color, draw some basic shapes, and read keypresses.
*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>
#include <linux/fb.h>

typedef unsigned short color_t;
int fd;
struct fb_var_screeninfo virRes;
struct fb_fix_screeninfo bitDep;
size_t length;
void *mapAddr;
struct termios termSet;
size_t x_res;
size_t y_res;

/*
    In this function, any work necessary to initialize the graphics
    library is done.
*/
void init_graphics() {
    fd = open("/dev/fb0", O_RDWR);
    ioctl(fd,FBIOGET_VSCREENINFO,&virRes);
    ioctl(fd,FBIOGET_FSCREENINFO,&bitDep);
    x_res = bitDep.line_length;
    y_res = virRes.yres_virtual;
    length = x_res * y_res * sizeof(size_t);
    mapAddr = mmap(NULL, length, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    ioctl(0,TCGETS,&termSet);
    termSet.c_lflag &= ~(ECHO | ICANON);
    ioctl(0,TCSETS,&termSet);
}

/*
    This function undoes whatever it is that needs to be cleaned up
    before the program exits. Many things will be cleaned up
    automatically if forgotten, for instance files will be closed and
    memory can be unmapped. It’s always a good idea to do it with
    close() and munmap() though.
    What will need to be done is to make an ioctl() to reenable key
    press echoing and buffering.
*/
void exit_graphics() {
    close(fd);
    munmap(mapAddr, length);
    ioctl(0,TCGETS,&termSet);
    termSet.c_lflag &= ~(ECHO | ICANON);
    ioctl(0,TCSETS,&termSet);
}

/*
    An ANSI escape code will be used to clear the screen rather than
    blanking it by drawing a giant rectangle of black pixels. ANSI
    escape codes are a sequence of characters that are not meant to be
    displayed as text but rather interpreted as commands to the
    terminal. The string “\033[2J” can be printed to tell the terminal
    to clear itself. Note that when calculating the length of that
    string, \033 is an escape sequence.
*/
void clear_screen() {
    write(1,"\033[2J",4);
}

/*
    Key press input will be used and a single character will be read
    using the read() system call. However, read() is blocking and that
    will cause the program to not be able to draw unless the user has
    typed something. Instead, we want to know if there is a keypress at
    all, and if so, read it. This can be done using the Linux
    non-blocking system call select().
*/
char getkey() {
    char key;
    fd_set rfds;
    int retVal;
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = 0;
    key = 0;
    FD_ZERO(&rfds);
    FD_SET(fd,&rfds);

    retVal = select(1,&rfds,NULL,NULL,&tv);
    if (retVal != -1) {
        read(fd,&key,1);
    }
    return key;
}

/*
    The system call nanosleep() will be used to make the program sleep
    between frames of graphics being drawn. It has nanosecond precision,
    but that level of granularity isn't needed. Instead, sleep will be
    used for a specified number of milliseconds and just multiply that
    by 1,000,000. We do not need to worry about the call being
    interrupted and so the second parameter to nanosleep() can be NULL.
*/
void sleep_ms(long ms) {
    struct timespec req;
    req.tv_nsec = ms*1000000;
    nanosleep(&req, NULL);
}

/*
    This is the main drawing code, where the work is actually done. We
    want to set the pixel at coordinate (x, y) to the specified color.
    The given coordinates will be used to scale the base address of the
    memorymapped framebuffer using pointer arithmetic. The frame buffer
    will be stored in row-major order, meaning that the first row
    starts at offset 0 and then that is followed by the second row of
    pixels, and so on.
*/
void draw_pixel(int x, int y, color_t c) {
    if (x >= x_res || y >= y_res || x < 0 || y < 0) {
        return;
    }
    color_t *currAddr = (color_t*)mapAddr + (y*x_res) + x;
    currAddr[0] = c;
}

/*
    Using draw_pixel, this function makes a rectangle with corners
    (x1, y1), (x1+width,y1), (x1+width,y1+height), (x1, y1+height).
*/
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    int i;
    int j;
    for (i = y1; i <= y1 + height; i++) {
        for (j = x1; j <= x1 + width; j++) {
            if (i == y1 || j == x1 || i == (y1 + height) || j == (x1 + width)) {
                draw_pixel(j,i,c);
            }
        }
    }
}

/*
    This function draws the string with the specified color at the
    starting location (x,y) – this is the upper left corner of the
    first letter.
    A font encoded into an array is provided. It is in a header file
    iso_font.h that is #include'd into the code.
    Each letter is 8x16 pixels. It is encoded as 16 1-byte integers.
    The array iso_font defined as a global in the iso_font.h file is
    indexed by the ASCII character value times the number of rows, so
    the 16 values for the letter ‘A’ (ASCII 65) can be found at indices
    (65*16 + 0) to (65*16 + 15).
    Using shifting and masking, each bit of each of the 16
    rows will be gone through and a pixel will be drawn at the
    appropriate coordinate if the bit is 1.
    It may be convenient to break this into two functions, one for
    drawing a single character, and then the required one for drawing
    all of the characters in a string. Since strlen() is a C Standard
    Library function, it won't be used. It will be iterated until ‘\0’
    is found.
*/
void draw_text(int x, int y, const char *text, color_t c) {

}