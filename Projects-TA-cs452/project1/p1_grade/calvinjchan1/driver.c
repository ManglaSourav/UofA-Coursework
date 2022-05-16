/*
Filename: driver.c
Author: Ember Chan
Course: CSC452
Purpose: Demonstrate the capabilites of the graphics library

*/

#include "graphics.h"
//Note: screen is 640x480

color_t WHITE = 65535;
color_t BLACK = 0;
color_t BLUE = 31;

float ACCEL = 1.0f;
float MAX_SPEED = 3.0f;

void draw_char(int x, int y){
	draw_rect(x-50, y-50, 100, 100, WHITE);
	draw_rect(x-25, y-100, 50, 50, WHITE);
	draw_pixel(x-10, y-75, BLACK);
	draw_pixel(x+10, y-75, BLACK); 
}


int main(int argc, char** argv){
	
	char key;
	float xvel = 0.0f;
	float yvel = 0.0f;
	float char_x = 640/2;
	float char_y = 480/2;
	
	init_graphics();
	do {
		key = getkey();
		//Apply acceleration to character according to key press
		if(key == 'w') yvel -= ACCEL;
		else if (key == 's') yvel += ACCEL;
		else if (key == 'a') xvel -= ACCEL;
		else if (key == 'd') xvel += ACCEL;
		
		//Bound speed of character
		if(yvel > MAX_SPEED) yvel = MAX_SPEED;
		if(yvel < -MAX_SPEED) yvel = -MAX_SPEED;
		if(xvel > MAX_SPEED) xvel = MAX_SPEED;
		if(xvel < -MAX_SPEED) xvel = -MAX_SPEED;
		
		//Update character position
		char_x += xvel;
		char_y += yvel;
		
		//Bound character position
		if(char_x > 590) char_x = 590;
		if(char_x < 50) char_x = 50;
		if(char_y < 50) char_y = 50;
		if(char_y > 430) char_y = 430;
		
		clear_screen();
		draw_char(char_x, char_y);
		draw_text(10, 10, "PRESS Q TO QUIT", BLUE);
		draw_text(10, 26, "WASD TO MOVE", BLUE);
		sleep_ms(33);
	} while (key != 'q');
	
	
	exit_graphics();
	return 0;
}
