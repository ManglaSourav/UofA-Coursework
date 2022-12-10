#include "graphics.h"

/* driver.c
 * This is my custom driver to show all required function calls.
 * It uses square.c as a base and builds upon it by also utilizing clear_screen(),
 * draw_text(), and one custom special effect function slow_write().
 * All required functions for the assignment are used in this driver.
 */


void explode(int x, int y) {
	int i;
	slow_write('B',x, y+10, 500);
	slow_write('A',x, y+25, 500);
	slow_write('N',x, y+40, 500);
	slow_write('G',x, y+55, 500);
	slow_write('!',x, y+70,500);
	for (i = 0; i < 100; i+=3) {
		draw_rect(x+i,y,20,20,65330);
		sleep_ms(10);
		draw_rect(x,y+i,20,20,65330);
		sleep_ms(10);
		draw_rect(x-i,y,20,20,65330);
		sleep_ms(10);
		draw_rect(x,y-i,20,20,65330);
		sleep_ms(10);
	}
	clear_screen();
}


int main(int argc, char** argv) {
	
	init_graphics();

	char key;
	int x = 640/2;
	int y = 480/2;
	do
	{
		// Draw the instructions
		draw_text(80, 40, "Move with w a s d", 65336);
		draw_text(80, 60, "Press x to EXPLODE", 65336);
		draw_text(80, 80, "Press q to quit", 15);

		// Draw a black rectangle to erase the old one
		draw_rect(x, y, 20, 20, 0);
		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		else if(key == 'x') explode(x,y);

		//Draw a blue rectangle
		draw_rect(x, y, 20, 20, 15);
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
