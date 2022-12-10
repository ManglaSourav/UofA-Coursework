/*
 * File Name: driver.c
 * Author: Michael Tuohy
 * Class: CSc 452
 * NetID: michaeltuohy@email.arizona.edu
 * Description: This is a driver program set to showcase the
 * correct implementation of the methods required by the project. There
 * are three programs to show this: box, key, and word. To run these
 * programs, you need to specify which one by passing in an argument.
 * The three accepted arguments are "-box", "-key", and "-word". These
 * programs aren't really interesting, and mostly just serve to showcase
 * the library.c program. 
 **/ 

#include "graphics.h"
#include <stdio.h>
#include <string.h>

#define WHITE 0xffff
#define RED 0xf800
#define GREEN 0x07e0
#define BLUE 0x001f
// This driver has a couple of different command line arguments that it recognizes.
// If the flag -box is set, the program calls the box method
// If the flag -key is set, the program calls the key method
// If the flag -word is set, the program calls the word method
// If no flag is set, the program defaults to the -box method


// This program is similar to the sample program, except instead of moving the box, it will
// make the rectangle bigger every time you press a, s, w, or d
void box() {
	char key;
	int x, y, width, height, up, down, left, right;

	init_graphics();

	x = (640 - 20) / 2;
	y = (480 - 20) / 2;
	width = 20;
	height = 20;
	up = 0;
	down = 0;
	left = 0;
	right = 0;

	do {
		draw_rect( (x-left), (y-up), (width + right), (height + down), RED );
		key = getkey();
	
		if(key == 'w') {
			up += 10;
			height += 10;
		} else if(key == 's') {
			down += 10;
		} else if(key == 'a') {
			left += 10;
			width += 10;
		} else if(key == 'd') {
			right += 10;
		}

		sleep_ms(20);	
		

	} while(key != 'q');

	exit_graphics();
}


// This program reads the users keys, and updates letter shown on screen to match the key
// that was last pressed, with the exception of q, which will stop the program
void key() {
	char key;
	int x, y;
	
	x = (640 - 20) / 2;
	y = (640 - 20) / 2;

	init_graphics();
	clear_screen();
	do {
		key = getkey();
		if(key != 0) {
			draw_rect(x, y, 8, 16, 0);
			draw_char(x, y, key, BLUE);
		}
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();
}

// This method will prompt the user for a word (less than 10 letters),
// Then display the word on the screen. The user can then press
// 'a' or 'd' to change the color of the word on screen. Pressing 'q'
// will end the program
void word() {
	char key;
	int color_num, x, y;
	color_t curr_color;
	char word[11];

	printf("Enter a word that's less than 10 letters:");
	fgets(word, 11, stdin);

	x = (640 / 2) - (8 * strlen(word));
	y = 480 / 2;

	init_graphics();
	clear_screen();
	color_num = 0;
	do {
		key = getkey();
		if(key == 'a') {
			if(color_num == 0) {
				color_num = 3;
			} else {
				color_num--;
			}
		} else if(key == 'd') {
			if(color_num == 3) {
				color_num = 0;
			} else {
				color_num++;
			}
		}

		switch(color_num) {
			case 0:
				curr_color = WHITE;
				break;
			case 1:
				curr_color = RED;
				break;
			case 2:
				curr_color = GREEN;
				break;
			case 3: 
				curr_color = BLUE;
				break;
		}
		draw_text(x, y, word, curr_color);
		sleep_ms(20);

	} while(key != 'q');
		

	exit_graphics();
}

int main(int argc, char *argv[]) {	

	if(argc == 1) {
		box();
	} else if(argc == 2) {
		// Check argv[2] for option
		if(strcmp(argv[1], "-box") == 0) {
			box();
		} else if(strcmp(argv[1], "-key") == 0) {
			key();
		} else if(strcmp(argv[1], "-word") == 0) {
			word();
		} else {
			box();
		}
	} else {
		printf("Too many arguments supplied, terminating\n"); return 1;
	}

	return 0;
}
