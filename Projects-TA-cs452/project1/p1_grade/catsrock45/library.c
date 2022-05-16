/*
* File: library.c
* Author: Amin Sennour
* Purpose: define the functions required to implment a small graphics library 
*          based on the header file specified in graphics.h
*       
*          see "graphics.h" for all function defninitions and documentation
*/


#include "graphics.h"
#include "iso_font.h"


// State variable for the file descriptor 
int fd;
// State variable for the framebuffer
color_t* fb;
// State variable for the framebuffers length
size_t fb_len; 
// State variables for the width and height of the framebuffer
size_t fb_width;
size_t fb_height;


/**
 * Purpose : turn 3 short rgb values into a color_t
 * Params :
 *      r : red intensity (0-31)
 *      g = green intensity (0-63) 
 *      b = blue intensity (0-31)
 * Return : the color_t made by the 3 input values
 */
color_t make_color(short r, short g, short b) {
    return ((r&0x1F)<<11) | ((g&0x3F)<<5) | (b&0x1F);
}


/**
 * Purpose : initialize our graphics library
 * Params : none
 * Return : void
 */
void init_graphics() {

    // open the framebuffer 
    fd = open("/dev/fb0", O_RDWR);
    
    // get the framebuffers metadata 
    struct fb_var_screeninfo var_screen_info;
    struct fb_fix_screeninfo fix_screen_info;
    ioctl(fd, FBIOGET_VSCREENINFO, &var_screen_info);
    ioctl(fd, FBIOGET_FSCREENINFO, &fix_screen_info);

    // map the framebuffer into our local address space
    // note, fb_len is in bytes 
    fb_len = var_screen_info.yres_virtual * fix_screen_info.line_length;
    fb = mmap(NULL, fb_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    // disable keypresses 
    struct termios terminal_settings; 
    ioctl(0, TCGETS, &terminal_settings);
    terminal_settings.c_lflag = terminal_settings.c_lflag & !ICANON;
    terminal_settings.c_lflag = terminal_settings.c_lflag & !ECHO;
    ioctl(0, TCSETS, &terminal_settings);

    // convert fb_len from bytes to color_t
    fb_len = fb_len;
    // define the width
    fb_width = fix_screen_info.line_length / 2;
    // define the height
    fb_height = var_screen_info.yres_virtual;
}


/**
 * Purpose : clean up persistent changes made by our library
 *           IE, rest the termianl settings to their defaults
 * Params : none
 * Return : void
 */
void exit_graphics() {
    // close the file 
    close(fd);
    // unmap the framebuffer
    munmap(fb, fb_len);

    // re-enable keypresses 
    struct termios terminal_settings; 
    ioctl(0, TCGETS, &terminal_settings);
    terminal_settings.c_lflag = terminal_settings.c_lflag | ICANON;
    terminal_settings.c_lflag = terminal_settings.c_lflag | ECHO;
    ioctl(0, TCSETS, &terminal_settings);
}


/**
 * Purpose : clear the screen using an escape code
 * Params : none
 * Return : void
 */
void clear_screen() {
    char* clear_code = "\033[2J";
    write(1, clear_code, 4);
    return;
}


/**
 * Purpose : get the current key pressed by the user. 
 *           if there is no key currently being pressed then return '\0'.
 * Params : none
 * Return : the current key being pressed, or NULL. 
 */
char getkey() {
    // define the fd for standard in 
    int fd_stdin = 0;

    // define the timeout
    struct timeval time_to_wait;
    time_to_wait.tv_sec = 0;
    time_to_wait.tv_usec = 0;
    
    // configure the fd set
    fd_set fd_set_stdin;
    FD_ZERO(&fd_set_stdin);
    FD_SET(fd_stdin, &fd_set_stdin);
    
    // perform the select
    int result = select(fd_stdin + 1, &fd_set_stdin, NULL, NULL, &time_to_wait);

    // if the select is successful the get a key
    char ret = '\0';
    if (result > 0) {
        read(fd_stdin, &ret, 1);
    }

    return ret;
}


/**
 * Purpose : sleep for a given number of miliseconds
 * Params : 
 *      ms : the miliseconds to sleep for   
 * Return : void
 */
void sleep_ms(long ms) {
    long seconds = ms / 1000;
    long ns = (ms % 1000) * 1000000L;
    struct timespec time = {
        seconds, ns
    };
    int result = nanosleep(&time, NULL);
}


/**
 * Purpose : draw a pixel at the give x,y coordinates with the given color
 * Params : 
 *          x : the x coord
 *          y : the y coord 
 *          c : the color to draw the pixel
 * Return : void
 */
void draw_pixel(int x, int y, color_t c) {
    long location = (fb_width * y) + x;
    if (location < (fb_width * fb_height)) {
        *(fb + location) = c;
    }  
}


/**
 * Purpose : draw a rectange with the upper left corner at x1,y1 and with the 
 *           given width and height, shaded using the given color. 
 * Params : 
 *         x1 : the x coord of the upper left corner of the square
 *         y1 : the y coord of the upper left corner of the square
 *      width : the width of the square 
 *     height : the height of the square
 *          c : the color to shade the square
 * Return : void
 */
void draw_rect(int x1, int y1, int width, int height, color_t c) {
    int y;
    for (y = y1; y <= (y1+height); y++) {
        int x;
        for (x = x1; x <= (x1+width); x++) {
            draw_pixel(x,y,c);
        }
    }
}


/**
 * Purpose : the nth bit of a given unsigned char
 * Params :
 *      c : the unsigned char to get the nth bit of
 *      n : the position from which to get the bit of c
 * Return :
 *      the nth bit of c
 */
char get_nth_bit(unsigned char c, char n) {
  char tmp = 1<< n;
  return (c & tmp) >> n;
}


/**
 * Purpose : draw a simple horizontal line of a letter starting at (x,y)
 * Params :
 *      x : the x coord to start the line
 *      y : the y coord to start the line
 *   line : the char representing the line to draw (iso encoding)
 *      c : the color to draw the line
 * Return : void
 */
void draw_letter_line(int x, int y, char line, color_t c) {
    int i;
    for (i = 0; i < 8; i++) {
        if (get_nth_bit(line, i)) {
            draw_pixel(x + i, y, c);
        }
    }
}


/**
 * Purpose : draw the given letter in color c with it's top lefthand corner at
 *           (x,y)
 * Params :
 *          x : the x coord of the top left corner of the tetter
 *          y : the y coord of the top left corner of the letter
 *     letter : the letter to draw
 *          c : the color to draw the letter in 
 * Return : void
 */
void draw_letter(int x, int y, char letter, color_t c) {
    int i;
    for (i = 0; i < 16; i++) {
        int index = letter * 16 + i;
        draw_letter_line(x, y+i, *(iso_font + index), c);
    }
}


/**
 * Purpose : draw the string specified by text in color c from left to right 
 *           with the top left corner of the first letter being at position 
 *           (x,y)
 * Params : 
 *          x : the x coord of the top left corner of the first letter
 *          y : the y coord of the top left corner of the first letter
 *       text : the text to write
 *          c : the color to write the text in 
 * Return : void 
 */
void draw_text(int x, int y, const char *text, color_t c) {
    char letter;
    do {
        letter = *text;
        draw_letter(x, y, letter, c);
        text += 1;
        x += 16; 
    } while (letter != '\0');
}