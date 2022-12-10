/**
 * @file driver.c
 * @author Ta My Linh
 * 
 * This program will print out to the screne what you type. 
 * 
 */
#include "library.h"

int main(int argc, char** argv)
{
	int i;
	init_graphics();
	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;
	int charX = 11;
	int charY = 30;
	clear_screen();
    draw_text(0, 0, "TA MY LINH, enter q to quit", RGB(255, 255, 255));
    draw_rect(10, 20, 400, 400, RGB(255, 0, 0));
	do
	{
		key = getkey();
		draw_char(charX, charY, key, RGB(180, 50, 50));
		charX += 8;
        if (charX > 400) {
            charX = 11;
            charY += 20;
        } 
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
