
/**
 * Author: Zachary Florez 
 * Course: CSC 452 
 * File: graphics.h
 * Description: This file is the graphics header that is used for the first 
 *              project. We will have all the function prototypes, color_t, and 
 *              the RGB macro needed.
 * 
 */


/* 
 * Macro to encode color_t from three RGB values using 
 * bit shifting and masking to make single 16-bit number. 
 *
 * Since 16 isn't evenly divisble by 3 the layout will be:
 *      upper  5 bits to RED    (0-31) (1f == 11111)
 *      middle 6 bits to GREEN  (0-63) (3f == 111111)
 *      lower  5 bits to BLUE   (0-31) (1f == 11111)
 */
#define RGB(r, g, b) ( ((r & 0x1f) << 11) | ((g & 0x3f) << 5) | (b & 0x1f) )


// unsigned short for color type (2 bytes, 16 bits)
typedef unsigned short int color_t; 


// Function Prototypes
void init_graphics(); 

void exit_graphics();

void clear_screen(); 

char get_key(); 

void sleep_ms(long ms); 

void draw_pixel(int x, int y, color_t color);

void draw_rect(int x1, int y1, int width, int height, color_t c); 

void draw_text(int x, int y, const char* text, color_t c);
