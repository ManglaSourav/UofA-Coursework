//* File: driver.c
// Purpose: This file displayes the varying exponential speed of 
// 7 cars represented by squares
//*
typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);

int main(int argc, char** argv)
{
	int i;

	init_graphics();

	char key;
	int x = 20;
	int y = 450;

	do
	{
		//draw a black rectangle to erase the old one
		draw_rect(x, y, 20, 20, 0);
		draw_rect(x*0.5, y, 20*0.5, 20, 0);
		draw_rect(x*1.5, y, 20*1.5, 20, 0);
		draw_rect(x*2, y, 20*2, 20, 0);
		draw_rect(x*2.5, y, 20*2.5, 20, 0);
		draw_rect(x*3, y, 20*3, 20, 0);
		draw_rect(x*4, y, 20*4, 20, 0);



		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		//draw a blue rectangle
		draw_rect(x, y, 20, 20, 15);
		draw_rect(x*0.5, y, 20*0.5, 20, 15);
		draw_rect(x*1.5, y, 20*1.5, 20, 15);
		draw_rect(x*2, y, 20*2, 20, 15);
		draw_rect(x*2.5, y, 20*2.5, 20, 15);
		draw_rect(x*3, y, 20*3, 20, 15);
		draw_rect(x*4, y, 20*4, 20, 15);



		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
