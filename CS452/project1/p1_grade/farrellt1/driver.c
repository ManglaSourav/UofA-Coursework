/*
Author: Tristan Farrell

Description:
driver.c displays usage of each function from library.c

prototypes come from graphics.h
*/

#include "graphics.h"

int main(int argc, char** argv)
{
	int i = 0;
	int c = 0;
	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	init_graphics();

	clear_screen();

	draw_text(5,5,"TYPE: t=text, wasd=move rect, c=clear, e=sleep",65535);

	do
	{
		draw_rect(x, y, 50, 50, 65535);
		key = getkey();
		if(key == 'w'){
			//draw a black rectangle to erase the old one
			draw_rect(x, y, 50, 50, 0);
			y-=25;
		}
		else if(key == 's'){
			draw_rect(x, y, 50, 50, 0);
			y+=25;
		}
		else if(key == 'a'){
			draw_rect(x, y, 50, 50, 0);
			x-=25;
		}
		else if(key == 'd'){
			draw_rect(x, y, 50, 50, 0);
			x+=25;
		}
		else if(key == 't'){
			//stack text with new color
			i++;
			c++;
			draw_text(20,20*i,"Look at this text!",1000*c);
		}else if(key == 'c'){
			//clear screen and reset text position
			i=0;
			clear_screen();
			draw_text(5,5,"TYPE: t=text, wasd=move rect, c=clear, e=sleep",65535);
		}
		else if(key == 'e') sleep_ms(100);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
