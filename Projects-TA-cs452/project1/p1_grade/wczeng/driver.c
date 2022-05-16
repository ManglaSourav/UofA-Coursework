/*
 * Author: Winston Zeng
 * File: driver.c
 * Class: CSC 452, Spring 2022
 * Project 1: Graphics Library
 */

#include "library.h"

int main(int argc, char** argv) {
	int i;
	init_graphics();
	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	do {
		clear_screen();
		key = getkey();
		if (key =='w') y-=10;
		else if (key == 's') y+=10;
		else if (key == 'a') x-=10;
		else if (key == 'd') x+=10;
		
		const char *sus = "sus";
		draw_text(x, y, sus, 2000);
		// first row
		fill_rect(x-20, y+50, 60, 15, 63488);
		// second row
		fill_rect(x-40, y+65, 100, 15, 63488);
		// third row
		fill_rect(x-80, y+80, 100, 15, 63488);
		fill_rect(x+20, y+80, 40, 15, 65535);
		// fourth row
		fill_rect(x-80, y+95, 80, 15, 63488);
		fill_rect(x, y+95, 80, 15, 65535);
		// fifth row
		fill_rect(x-80, y+110, 100, 15, 63488);
		fill_rect(x+20, y+110, 40, 15, 65535);
		// sixth row
		fill_rect(x-80, y+125, 140, 15, 63488);
		// seventh row
		fill_rect(x-80, y+140, 140, 15, 63488);
		// eighth row
		fill_rect(x-80, y+155, 140, 15, 63488);
		// ninth row
		fill_rect(x-40, y+170, 40, 15, 63488);
		fill_rect(x+20, y+170, 40, 15, 63488);
		// tenth row
		fill_rect(x-40, y+185, 40, 15, 63488);
		fill_rect(x+20, y+185, 40, 15, 63488);
		sleep_ms(20);
	} while(key != 'q');
		
	exit_graphics();
	return 0;	
}
