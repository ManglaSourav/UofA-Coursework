 /* File: driver.c
 *
 * Author: Zachary Taylor
 * NetID: ztaylor
 * Class: CSC 452
 * Assignment: HW1
 *
 */
#include "graphics.h"
/* the user can change the color of the rectangle that is being drawn
*	t and y for blue values
*	g and h for green values
*	b and n for red values
*	movement is controlled with w,a,s,d
*	and then you can draw a pretty picture.
*/
int main(int argc, char** argv)
{
	int i;

	init_graphics();

	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;
	int color = 0;
	draw_text(10,10, "Zach's Driver", 8000);
	do
	{
		
		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		else if(key == 't') color+=1;
		else if(key == 'y') color-=1;
		else if(key == 'g') color+=32;
		else if(key == 'h') color-=32;
		else if(key == 'b') color+=2048;
		else if(key == 'n') color-=2048;
		
		draw_rect(x, y, 20, 20, color);
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
