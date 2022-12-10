#include "graphics.h"

/*
 * File: driver.c
 * Author: Rebekah Julicher
 * Purpose: Demonstrates graphics library functions working
*/

int main(int argc, char** argv){
	int i;

	init_graphics();

	draw_text(10, 10, "WASD to move cursor - 0-9 to change color - J and K to resize brush", 0xFFFF);
	draw_text(10, 30, "M to swap between regular and x-ray modes, Q to quit", 0xFFFF);

	// Just to show this working
	draw_pixel(5, 10, 0xFFFF);

	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	int c = 15;
	int brush_size = 20;
	int mode = 1;

	do{
		key = getkey();
		if (key == 'w') y -= 5;
		else if(key == 's') y += 5;
		else if (key == 'a') x -= 5;
		else if (key == 'd') x += 5;

		// Change colors
		else if (key == '1') c = 0;
		else if (key == '2') c = 15;     // Blue
		else if (key == '3') c = 0xBEF;  // Turquoise
		else if (key == '4') c = 0xBE3;  // Green
		else if (key == '5') c = 0xFBE3; // Orange
		else if (key == '6') c = 0xDEA8; // Yellow
		else if (key == '7') c = 0x7800; // Red
		else if (key == '8') c = 0x780F; // Purple
		else if (key == '9') c = 50000;  // Pink
		else if (key == '0') c = 0xFFFF; // White

		else if (key == 'j' && brush_size > 10) brush_size -= 5;
		else if (key == 'k' && brush_size < 50) brush_size += 5;

		else if (key == 'm') mode = !mode;

		if (mode == 0) draw_rect(x, y, brush_size, brush_size, c);
		else fill_rect(x, y, brush_size, brush_size, c);
		
		sleep_ms(20);

	} while(key != 'q');

	clear_screen();

	exit_graphics();
	return 0;
}
