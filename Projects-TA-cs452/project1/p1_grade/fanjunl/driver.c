#include "graphics.h"
//#include <stdio.h>
//void clear_screen();
//void exit_graphics();
//void init_graphics();
//char getkey();
//void sleep_ms(long ms);
//
//void draw_pixel(int x, int y, color_t color);
//void draw_rect(int x1, int y1, int width, int height, color_t c);

int main(int argc, char** argv)
{
	int i;

	init_graphics();

	char key;
	int x = (640 - 20) / 2;
	int y = (480 - 20) / 2;
	draw_rect(0, 0, 639, 479, 65535);
	draw_text(20, 20, "hello world", 63488);
	draw_text(20, 40, "Happy,NewYear", 2016);
	draw_text(20, 100, "Using the keys 'wasd' to draw.", 2016);
	do
	{
		//draw a black rectangle to erase the old one
		draw_rect(x, y, 20, 20, 0);
		//printf("test\n");
		key = getkey();
		if (key == 'w') y -= 10;
		else if (key == 's') y += 10;
		else if (key == 'a') x -= 10;
		else if (key == 'd') x += 10;
		//draw a blue rectangle
		draw_rect(x, y, 20, 20, 63488);
		sleep_ms(20);
	} while (key != 'q');
	clear_screen();
	exit_graphics();
	return 0;

}
