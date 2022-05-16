#include "graphics.h"
#include <stdio.h>
#include <stdlib.h>
/*
* Author: Kyle Smith
* Driver program to showcase graphics library
*/
int main() {
	char key;


	color_t c = 15;
	clear_screen();
	
	init_graphics();
	char str[] = "Wanna see something cool?";
	char str1[] = "Press w or s (q for quit)";
	char str2[] = "Rectangle Rainbow. (Epilepsy Warning)";
	
	draw_text(10,10,str,15);
	sleep_ms(2000);
	clear_screen();
	draw_text(10,10,str2,51515);
	draw_text(450,300,str1,15);
	draw_rect(100,100,300,300,15);
	do {
		key = getkey();
		if (key == 'w') c+= 5432;
		else if (key == 's') c-=5432; 
		clear_screen();
		draw_rect(100,100,300,300,c);
		sleep_ms(15);
	} while(key != 'q');

	//sleep_ms(2000);
	clear_screen();
	exit_graphics();
	return 0;
}
