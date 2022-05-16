/*
Author: Christian Trejo
Course: CSC452
Assignment: Project 1: Graphics Library
File: driver.c
Purpose: Test functions written in library.c. Allows the user
	 to paint on the screen using red, green, and blue colors
	 by moving the square around using keys awsd.
*/

#include "library.h"

int main(int argc, char** argv){

	init_graphics();	//Initialize graphics

	color_t color = getRGB(0,63,0);	//Initial color Green

	int x = 200;			//Starting x
	int y = 100;			//Starting y
	int height = 16;		//Height of ASCII char
	int border_length = 346;	//Border length
	int border_thickness = 10;	//Border thickness
	int x_buffer = 50;		//Buffer in x direction
	int y_buffer = 25;		//Buffer in y direction

	//Top border
	draw_rect(x-x_buffer, y-y_buffer, border_length, 
	border_thickness, color);

	//Bottom border
	draw_rect(x-x_buffer, y+height*6, border_length, 
	border_thickness, color);

	//Left side of border
	draw_rect(x-x_buffer, y-y_buffer, border_thickness, 
	height*6+y_buffer, color);

	//Right side of border
	draw_rect(x-x_buffer+border_length-border_thickness, y-y_buffer, 
	border_thickness, height*6+y_buffer, color);

	//Instructions on how to draw
	draw_text(x, y, "Welcome to Paint Class", color);
	draw_text(x, y+height*2, "Use awsd to move square around", color);
	draw_text(x, y+height*3, "b = blue, g = green, r = red", color);
	draw_text(x, y+height*4, "c = clear screen, q = quit", color);


	sleep_ms(8000);		//Sleep for 8 seconds
	clear_screen();		//Clear screen

	//Start square in middle of screen
	x = (640-10)/2;
	y = (480-10)/2;
	char key;

	do {
		draw_rect(x, y, 10, 10, color);
		key = getkey();
		if(key == 'w') y-=10;		//Move up
		else if(key == 's') y+=10;	//Move down
		else if(key == 'a') x-=10;	//Move right
		else if(key == 'd') x+=10;	//Move left
		else if(key == 'b') color = getRGB(0,0,15); //Blue
		else if(key == 'g') color = getRGB(0,63,0); //Green
		else if(key == 'r') color = getRGB(15,0,0); //Red
		else if(key == 'c') clear_screen();	//Clear screen
		sleep_ms(20);

	} while(key != 'q');		//Quit

	exit_graphics();		//Restore terminal settings
	return 0;
}
