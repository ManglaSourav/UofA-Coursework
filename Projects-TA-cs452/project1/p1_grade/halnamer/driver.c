#include "graphics.h"
int main(int argc, char** argv)
{
	int i;

	init_graphics();

	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;
	int x_direction = 1;
	int y_direction = 1;
	int prev_x = x;
	int prev_y = y;
	do
	{
		if (x >= 640-50 || x<=0 ){ 
			x_direction *= -1;
		}
		if(y >= 480-50 || y <= 0){
			y_direction *=-1;
		}
		//draw a black rectangle to erase the old one
		draw_text(0,420, "Press q to quit", CONVERT(255, 0, 0));
		//draw hideout rectangles
		draw_text(prev_x+5, prev_y+5,"GAME", 0);
		draw_rect(prev_x, prev_y, 50, 50, 0);
		key = getkey();

		//draw a blue rectangle
		draw_text(x+5, y+5,"GAME", CONVERT(x, y, y) );
		draw_rect(x, y, 50, 50, CONVERT(x, y, x));
		
		prev_x = x;
		prev_y =y;
		x+=x_direction*1;
		y+=y_direction*1;
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
