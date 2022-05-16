/**
 * @file driver.c
 * @author Luke Broadfoot (lucasxavier@email.arizona.edu)
 * @brief a simple program to show off each of the required functions
 * from my library.c file for Project 1: Graphics Library.
 * 
 * My program allows the user to draw using with 8 colors (9 if you count black)
 * Using [w,a,s,d] the user can move the 'curser' around, and using
 * [1,2,3,4,5,6,7,8] you can switch between 8 colors and use [0] to draw black
 * 
 * @version 1.0
 * @date 2022-02-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "graphics.h"
// red, orange, yellow, green, cyan, blue, indigo, violet
unsigned short colors[] = {0xf800, 0xfca0, 0xffe0, 0x07e0,
                           0x0eff, 0x001f, 0x0013, 0x480f};
// strings displayed on the screen
const char *quit_prompt = "Press 'q' to quit";
const char *move_prompt = "Type [w,a,s,d] to move";
const char *color_prompt = "Type [1,2,3,4,5,6,7,8,0] to change colors";

/**
 * @brief my version of the C Standard Library function
 * 
 * @param text a const char * of a string
 * @return int the length of the string
 */
int string_length(const char *text) {
	int res = 0;
	while (text[res] != '\0') { res++; }
	return res;
}

/**
 * @brief Used to display text in a flowing rainbow color
 * The text is auto centered
 * 
 * @param offset an offset for the color index, that is controlled externally
 * @param y the y position for the text
 * @param text a const char * of the string to be printed
 */
void rainbow(int offset, int y, const char *text) {
	int i = 0, x, len = string_length(text);
	x = (640/2) - (len*4);
	// This is a work around since draw_text wants a const char *
	char temp[2];
	temp[0] = 'x';
	temp[1] = '\0';
	// iterates over the text string
	for (i = 0; text[i] != '\0'; i++) {
		// updates temp to be the current character of text
		temp[0] = text[i];
		// calls library.c draw_text() which also calls draw_pixel()
		draw_text(x + (i*8), y, temp, colors[(i+offset) % 8]);
	}
}

int main() {
	// initializes graphics and clears the screen
	init_graphics();
	clear_screen();
	char key = '\0';
	// I'm just gonna assume the screen size doesn't change
	int i = 0, x = (640 / 2), y = (480 / 2);
	// calculates the starting x position for both strings such that they
	// are centered
	int text_x = (640 / 2) - (string_length(move_prompt) * 4);
	int text2_x = (640 / 2) - (string_length(color_prompt) * 4);
	// sets the starting color to cyan
	color_t c = 0x0eff;
	while (key != 'q') {
		// calls library.c getkey()
		key = getkey();
		
		// displays the all 3 prompts
		rainbow(i, 25, quit_prompt);
		draw_text(text_x, 40, move_prompt, 0xffff);
		draw_text(text2_x, 65, color_prompt, 0xffff);
		
		// updates the x and y values if [w,a,s,d] is pressed
		x = (key == 'd') ? x+10 : (key == 'a') ? x-10 : x;
		y = (key == 's') ? y+10 : (key == 'w') ? y-10 : y;

		// if the key pressed is 0-8, updates the color
		if (key == '0') { c = 0x0000; }
		else if (key == '1') { c = colors[0]; }
		else if (key == '2') { c = colors[1]; }
		else if (key == '3') { c = colors[2]; }
		else if (key == '4') { c = colors[3]; }
		else if (key == '5') { c = colors[4]; }
		else if (key == '6') { c = colors[5]; }
		else if (key == '7') { c = colors[6]; }
		else if (key == '8') { c = colors[7]; }
		// draws a new square
		draw_rect(x, y, 20, 20, c);
		// sleeps for 20ms
		sleep_ms(20);
		// goes to the next i offset for rainbow()
		i = (i+1) % 8;
	}
	// calls library.c exit_graphics() once 'q' is pressed
	exit_graphics();
	return 0;
}
