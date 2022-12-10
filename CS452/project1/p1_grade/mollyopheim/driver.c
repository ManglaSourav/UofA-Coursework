/**
* Author: Molly Opheim
* Course: CSc 452 Project 1
* File: driver.c - this is a simple driver to display the graphics 
functions from this assignment
*/ 
#include "graphics.h"

int main(int argc, char** argv) {
	int i;
	init_graphics();
	draw_text(300, 100, "hello world this is my square", 15);
	char keyPress;
	int x = 300;
	int y = 300;
	
	// prints a square that moves with w, s, a, d key press
	// until q is pressed
	do {
		draw_rect(x, y, 30, 30, 0);
		draw_pixel(x, y, 15);
		keyPress = getkey();
		if(keyPress == 'w') y -= 10;
		else if(keyPress == 's') y+= 10;
		else if(keyPress == 'a') x-=10;
		else if(keyPress == 'd') x +=10;
		draw_rect(x, y, 30, 30, 15);
		sleep_ms(20);
	
	} while(keyPress !='q');
	exit_graphics();
	return 0;
}
