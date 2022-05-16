// The header file for the graphics library
// Written by Ryan Alterman
// Revised 2/5/2022

#ifndef GRAPHICS_H
#define GRAPHICS_H

typedef unsigned short color_t;

/*
* This function is responsible for retrieving the framebuffer as well
* as disabling certain terminal settings.
*/
void init_graphics();

/*
* This function is responsible for reverting any changes we made to the
* terminal as well as close and release any resources that we asked for.
*/
void exit_graphics();

/*
* Uses an escape code to clear the screen. Ideally used every frame,
* which in our case is most likely at the beginning of our render loop.
*/
void clear_screen();

/*
* This function detects and returns if a key has been pressed.
*/
char getkey();

/*
* This function is responsible for making the system sleep
* for the specified number of milliseconds. As a result of
* the nanosleep() call, The max number of milliseconds we
* can sleep for is 999ms. If an invalid argument or some 
* other error prevents nanosleep from running, the error
* will be logged.
*/
void sleep_ms(long ms);

/*
* Draws the pixel on the screen at the provided location
* with the color specified.
*/
void draw_pixel(int x, int y, color_t color);

/*
* Draws a rectangle starting at the given coordinates
* with the provided color and dimensions.
*/
void draw_rect(int x1, int y1, int width, int height, color_t c);

/*
* Draws text on the screen at the given location with the
* specified color.
*/
void draw_text(int x, int y, const char* text, color_t c);

/*
* Converts the RGB color to a 16 bit integer.
*/
color_t convertRGB(color_t R, color_t G, color_t B);

#endif // GRAPHICS_H
