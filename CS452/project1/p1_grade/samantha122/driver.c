/* File: driver.c
 * Author: Samantha Mathis
 * Purpose: adds to square.c, by creating a driver that displays all functions of the graphics library
 * implemented in library.c. Creates a start to a game where your character can move around and when he picks
 * up the sword text appears on the screen.  
 */

#include "graphics.h"

int main(int argc, char** argv){

	init_graphics();
	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;
    int flag = 0;

	do
	{
        //draws the sword if it wasn't picked up
        if (flag == 0){
            draw_rect(400, 300, 10, 45, 65535);
            draw_rect(385, 345, 40, 10, 65535);
            draw_rect(400, 355, 10, 10, 65535);
        }

		//draw a black rectangle to erase the old one
		fill_rect(x, y, 20, 20, 0);
		key = getkey();
        //Moves the character 
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;

        //If the sword was picked up by the character clear the screen
        if (380 <= x &&  x <= 430 && 295 <= y && y <= 370 && flag == 0){
           flag = 1;
           clear_screen();
        }
        //Once sword is picked up display the text
        if (flag){
            draw_text(100, 100, "It is Dangerous to go alone! Take this Sword", 63888);
        }

        //Fixes the boundaries so that the character can wrap around the screen
        if (x > 640){
            x = 1;
        } if (x < 0){
            x = 639;
        }
        if (y > 480){
            y = 1;
        } if (y < 0){
            y = 479;
        }
		//draw a blue rectangle
		fill_rect(x, y, 20, 20, 15);
		sleep_ms(20);

	} while(key != 'q');

	exit_graphics();

	return 0;

}
