/*
* File: driver.c
* Purpose: Driver that runs and displays all the methods in library.c
*
* Author: Victor A. Jimenez Granados
* Date: Feb 04, 2022
*/

#include "graphics.h"

int main(int argc, char** argv)
{
	int i;

	init_graphics();

	char key;
	int x = (640 - 20) / 2;
	int y = (480 - 20) / 2;
	clear_screen();
	//Prints beginning text.
	draw_text(x, y, "Intial Rectangle Position.", 25);
	do
	{
		//draw a black rectangle to erase the old one
		draw_rect(x, y, 60, 60, 0);
		key = getkey();
		if (key == 'w') y -= 10;
		else if (key == 's') y += 10;
		else if (key == 'a') x -= 10;
		else if (key == 'd') x += 10;
		//draw a blue rectangle
		draw_rect(x, y, 60, 60, 25);
		sleep_ms(30);
	} while (key != 'q');
	//Prints ending text.
	draw_text(x, y, "Final Rectangle Position.", 25);
	exit_graphics();
	return 0;
}