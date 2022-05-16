/**
 * Author:  Mark Oakeson
 * Class: CSc 452
 * Instructor: Misurda
 * Project:  1
 * File: library.c
 *
 * Description: The purpose of this program is to implement a graphics library that can
 * set a pixel to a particular color, draw some basic shapes, and read keypresses.  It is run in the terminal,
 * using only linux system calls and no C library calls.  The program uses the "a, s, d, w" keys to move the 'painter'.
 * User presses 'q' to quit program.
 */

#include "iso_font.h" // Apple font

#include <fcntl.h> // Linux open command
#include <sys/ioctl.h> // get screen size and pixels
#include <sys/mman.h> // returns pointer to file
#include <linux/fb.h> // For the frame buffer
#include <sys/termios.h> // For the
#include <unistd.h> // For writing + reading


int frameBufferFile;
typedef unsigned short color_t;
color_t* frameBuffer;


struct fb_var_screeninfo varScreenInfo;
struct fb_fix_screeninfo fixScreenInfo;
struct termios terminalSettings;
int screenSize;

/**
 * Function uses ANSI escap code to tell the terminal to clear itself
 */
void clear_screen(){
    write(0, "\033[2J", 4);
}


/**
 * Function initilizes graphics by opening the frame buffer file.  Also retrieves information about
 * the bit depth and virtual resolution.  Program turns off keypress echo and buffering the keypress.
 * Then using the information retrieved, finds the total amount of memory needed for the framebuffer,
 * then writes it into memory using mmap.
 */
void init_graphics(){
    frameBufferFile = open("/dev/fb0", O_RDWR);
    ioctl(frameBufferFile, FBIOGET_VSCREENINFO, &varScreenInfo);
    ioctl(frameBufferFile, FBIOGET_FSCREENINFO, &fixScreenInfo);
    ioctl(STDIN_FILENO, TCGETS, &terminalSettings);
    terminalSettings.c_lflag = terminalSettings.c_lflag & ~ICANON;
    terminalSettings.c_lflag = terminalSettings.c_lflag & ~ECHO;
    ioctl(STDIN_FILENO, TCSETS, &terminalSettings);
    screenSize = varScreenInfo.yres_virtual * fixScreenInfo.line_length;
    frameBuffer = mmap(NULL, screenSize, PROT_WRITE, MAP_SHARED, frameBufferFile, 0);
    clear_screen();
}

/**
 * Function exits the raphics mode by clearing the screen, reseting the flags for keypress echo and
 * buffering, unmaps the address used for the framebuffer, and closes the file used for the framebuffer.
 */
void exit_graphics(){
    terminalSettings.c_lflag = terminalSettings.c_lflag | ICANON;
    terminalSettings.c_lflag = terminalSettings.c_lflag | ECHO;
    ioctl(STDIN_FILENO, TCSETS, &terminalSettings);
    munmap(frameBuffer, screenSize);
    clear_screen();
    close(frameBufferFile);
}



/**
 * Function retrieves the keypress from the user. It will read in one character
 * at a time, and the only characters used in the program for movement are 'a,s,d,w'.
 * The only other key is 'q' for exiting the program.
 * @return A character inputed by the user
 */
char getkey(){
    fd_set readInput;
    FD_ZERO(&readInput);
    FD_SET(STDIN_FILENO, &readInput);
    struct timeval timeToWait;
    timeToWait.tv_sec = 0;
    timeToWait.tv_usec = 0;
    char retval = '\0';
    if(select(STDIN_FILENO+1,&readInput, NULL, NULL, &timeToWait)){
        read(0, &retval, 1);
    }
    return retval;
}


/**
 * Function makes program sleep in between drawing frames on 'canvas'
 * @param ms A long used to determine how long to make the program sleep
 */
void sleep_ms(long ms){
    struct timespec toSleep;
    toSleep.tv_sec = 0;
    toSleep.tv_nsec = ms * 1000000;
    nanosleep(&toSleep,  NULL);
}


/**
 * Function draws a pixel on the canvas.  Checks if the user is trying to draw off of the canvas.
 * Program allows user to draw off of canvas, but anything drawn off of canvas will not be stored in memory,
 * the only thing is that the user's pointer will travel off of screen.
 * @param x An int for where to index horizontally into the framebuffer
 * @param y An int for where to index vertically in the framebuffer
 * @param color A color_t 16 bit RGB value
 */
void draw_pixel(int x, int y, color_t color){
    if(x < 0 || y < 0 || x >= varScreenInfo.xres_virtual || y >= varScreenInfo.yres_virtual){
        return;
    }
    int index = (y * varScreenInfo.xres_virtual) + x;
    color_t* pixel = frameBuffer + index;
    *pixel = color;
}


/**
 * Function Utilizes draw pixel to draw a rectangle onto the framebuffer
 * @param x1 An int marking the starting x coordinate in the framebuffer
 * @param y1 An int marking the starting y coordinate in the framebuffer
 * @param width An int for how wide the rectangle will be
 * @param height An int for how tall the rectangle will be
 * @param c A color_t 16 bit RGB value
 */
void draw_rect(int x1, int y1, int width, int height, color_t c){

    int startIndex = x1;
    int y;
    for(y = 0; y < height; y++) {
        int x;
        for (x = 0; x < width; x++) {
            draw_pixel(x1, y1, c);
            x1++;
        }
        y1++;
        x1 = startIndex;
    }
}


/**
 * Method helps the draw_text method by taking a character, and finding the encoded
 * version in 'iso_font.h' file.  Once found, it utiolizes print_pixel to loop and
 * print that character to the framebuffer
 * @param column An int to index into the column position
 * @param row An int to index into the row position
 * @param letter A character retrieved from the original string input by the user
 * @param color A color_t 16 bit RGB value
 */
void drawTextHelper(int column, int row, const char letter, color_t color){
    int r,c;
    for(r = 0; r < 16; r++){
        int curLetter = iso_font[letter*16 + r];
        for(c = 0; c < 8; c++){
            if((curLetter & 0x01) == 1){
                draw_pixel(column + c, row + r, color);
            }
	    curLetter >>=1;
        }
    }
}


/**
 * Method draws text to the frame buffer.  It iterates through the string input, sends a
 * character at a time to the helper function, drawTextHelper, then continues to iterate
 * through the string
 * @param x An int for where to index into the x coordinate
 * @param y An int for where to index into the y coordinate
 * @param text A string input by the user to print to the console
 * @param c A color_t 16 bit RGB value
 */
void draw_text(int x, int y, const char *text, color_t c){
    int i = 0;
    while(text[i] != '\0'){
        drawTextHelper(x+i*8, y, text[i], c);
        i++;
    }

}

