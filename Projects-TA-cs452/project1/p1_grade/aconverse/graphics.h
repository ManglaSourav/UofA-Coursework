/*
 * Author: Amber Charlotte Converse
 * File: graphics.h
 * Description: This file provides the prototypes for a graphics library
 * 	interface for a machine running Tiny Core Linux.
 */

#ifndef _graphics_h_
#define _graphics_h_

/*
 * This type def defines a 16-bit color. The upper 5 bits store red intensity
 * (0-31), the middle 6 bits store green intensity (0-63), and the low order 5
 * bits store blue intensity (0-31). 
 */
typedef unsigned short color_t;

/*
 * This function initializes the graphics library using the framebuffer for
 * the system.
 */
void init_graphics();

/*
 * This function closes out the graphics library and frees it for use by other
 * programs.
 */
void exit_graphics();

/*
 * This function clears the screen.
 */
void clear_screen();

/*
 * This function determines if a key has been pressed, and returns the key that
 * was pressed if it was. Otherwise, it returns NULL.
 *
 * RETURN:
 * char: the key that was pressed, NULL if no key was pressed.
 */
char getkey();

/*
 * This function pauses the program for the given amount of milliseconds.
 *
 * PARAM:
 * ms (long): the number of milliseconds to pause.
 */
void sleep_ms(long ms);

/*
 * This function changes the color of the pixel at (x,y) to the given color.
 *
 * PARAM:
 * x (int): the x-coordinate of the target pixel
 * y (int): the y-coordinate of the target pixel
 * color (color_t): the color to change the target pixel to (color_t type
 * 	defined above)
 */
void draw_pixel(int x, int y, color_t color);

/*
 * This function draws a rectangle with corners (x1,y1), (x1+width,y1),
 * (x1+width,y1+height), (x1,y1+height) with the given color.
 *
 * PARAM:
 * x1 (int): the x-coordinate of the base corner
 * y1 (int): the y-coordinate of the base corner
 * width (int): the width of the rectangle, extruded from the base corner
 * height (int): the height of the rectangle, extruded from the base corner
 * c (color_t): the color of the rectangle
 */
void draw_rect(int x1, int y1, int width, int height, color_t c);

/*
 * This function draws the given string with the specified color at the
 * starting location (x,y).
 *
 * PARAM:
 * x (int): the x-coordinate of the upper left corner of the first letter
 * y (int): the y-coordinate of the upper left corner of the first letter
 * text (const char*): the string to be drawn (will not be changed)
 * c (color_t): the color of the text
 */
void draw_text(int x, int y, const char *text, color_t c);

#endif
