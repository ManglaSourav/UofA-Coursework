/*
Author: Emilio Santa Cruz
Class: 452 @ Spring 2022
Professor: Dr. Misurda
Assignment: Project 1
Description: Serves as the driver to prove usage of all of the
required functions within library.c
*/

#include "graphics.h"

/*
Draws a certain amount of rectangles downwards on the canvas.
Parameters: x is where the rectangles are to start x-wise, y is where 
the rectangles are to start y-wise, c is the color to be drawn in, time 
is the amount of rectangles to be drawn down.
Pre-Conditions: x amount of rectangles are to be drawn down
Post-Conditions: x amount of rectangles are drawn
Return: None
*/
void drawDown(int x, int y, color_t c, int times){
	int i = 0;
	const int WIDTH = 50;
	while(i < times){
		draw_rect(x, y + 50 * i, 50, 50, c);
		i++;
	}
}

/*
Draws or erases an amogus onto the canvas at x,y
Parameters: erase is a boolean to determine to erase or not, x is where 
the starting x point is, y is where the starting y point is
Pre-Condition: An amogus is to be drawn or erased
Post-Condition: An amogus is drawn or erased
Return: None
*/
void amogus(int erase, int x, int y){
	color_t bodyColor;
	color_t visorColor;
	if(erase){
		bodyColor = 0;
		visorColor = 0;
	} else{
		int r = 255;
		int g = 0;
		int b = 0;
		bodyColor = r << 11 | g << 5 | b;
	
		r = 255;
		g = 255;
		b = 255;
		visorColor = r << 11 | g << 5 | b;
	}

	draw_text(200 + x, 100 + y, "S U S S Y", bodyColor);
	
	drawDown(200 + x, 200 + y, bodyColor, 3);
	drawDown(250 + x, 150 + y, bodyColor, 6);
	drawDown(300 + x, 150 + y, bodyColor, 4);
	drawDown(350 + x, 150 + y, bodyColor, 1);
	drawDown(350 + x, 200 + y, visorColor, 1);
	drawDown(350 + x, 250 + y, bodyColor, 4);
}

int main(){
	init_graphics();
	char key = 0;
	int x = 0;
	int y = 0;
	
	while(key != 'q'){
		amogus(1, x, y);
		key = getkey();
		if(key == 'w') y -= 10;
		else if(key == 's') y += 10;
		else if(key == 'a') x -= 10;
		else if(key == 'd') x += 10;
		amogus(0, x, y);
		sleep_ms(20);
	}
	
	exit_graphics();
}
