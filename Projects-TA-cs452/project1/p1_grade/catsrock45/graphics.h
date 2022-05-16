/*
* File: graphics.h
* Author: Amin Sennour
* Purpose: specify a header file to expose the methods and types needed to 
*          operate a graphics library
*/


#ifndef GRAPHICS_H
#define GRAPHICS_H


#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stddef.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/select.h>


typedef unsigned short color_t;


/**
 * Purpose : turn 3 short rgb values into a color_t
 * Params :
 *      r : red intensity (0-31)
 *      g = green intensity (0-63) 
 *      b = blue intensity (0-31)
 * Return : the color_t made by the 3 input values
 */
color_t make_color(short r, short g, short b);


/**
 * Purpose : initialize our graphics library
 * Params : none
 * Return : void
 */
void init_graphics(); 


/**
 * Purpose : clean up persistent changes made by our library
 *           IE, rest the termianl settings to their defaults
 * Params : none
 * Return : void
 */
void exit_graphics();


/**
 * Purpose : clear the screen using an escape code
 * Params : none
 * Return : void
 */
void clear_screen();


/**
 * Purpose : get the current key pressed by the user. 
 *           if there is no key currently being pressed then return '\0'.
 * Params : none
 * Return : the current key being pressed, or NULL. 
 */
char getkey();


/**
 * Purpose : sleep for a given number of miliseconds
 * Params : 
 *      ms : the miliseconds to sleep for   
 * Return : void
 */
void sleep_ms(long ms);


/**
 * Purpose : draw a pixel at the give x,y coordinates with the given color
 * Params : 
 *          x : the x coord
 *          y : the y coord 
 *          c : the color to draw the pixel
 * Return : void
 */
void draw_pixel(int x, int y, color_t c);


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
void draw_rect(int x1, int y1, int width, int height, color_t c);


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
void draw_text(int x, int y, const char *text, color_t c);


#endif