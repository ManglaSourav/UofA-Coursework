

typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

//#include "library.c"
#include <stdio.h>


int main(int argc, char** argv)
{
	int i;

	init_graphics();
	
	draw_text(180,240,"AB C",0xFFFF);
	draw_rect(100, 100, 40, 40, 0xFFFF);	
	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	do
	{
		//draw a black rectangle to erase the old one
		draw_rect(x, y, 25, 25, 0);
		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		//draw a blue rectangle
		
		draw_rect(x, y, 20, 20, 15);
		draw_rect(2, 15, 20, 20, 0);
		draw_text(200, 200, "hello world", 0xF9FA);
		
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
