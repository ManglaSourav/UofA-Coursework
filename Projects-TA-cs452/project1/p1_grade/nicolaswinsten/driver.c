#include "graphics.h"

int main(int argc, char** argv)
{
	init_graphics();
	clear_screen();
	
	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	do
	{
		//draw a black rectangle to erase the old one
		fill_rect(x, y, 14*16, 42, 0);
		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		//draw a green rectangle and label it
		draw_rect(x, y, 20, 20, rgb(10, 63, 10));
		draw_text(x, y+22, "this is my box", rgb(31, 63, 31));
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
