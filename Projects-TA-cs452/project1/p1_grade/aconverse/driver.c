/*
 * Author: Amber Charlotte Converse
 * File: driver.c
 * Description: This file uses the graphics.h library to display a frog that
 *	can walk around, grow, and stick its tongue out based on user input.
 *	There is some screen flicker, but I think this is only because of the
 *	amount of sprites on the screen because there is no flicker with
 *	square.c.
 */

#include "graphics.h"

/*
 * This function draws a cute little frog centered at (x,y).
 * 
 * PARAM:
 * x (int): the x-coordinate of the center of the frog
 * y (int): the y-coordinate of the center of the frog
 * color (color_t): the base color of the frog (body color) 
 * sf (float): the size factor, determines the size of the frog
 * tongue_extension (float): the percentage that the tongue is extended
 * 	(should be between 0.0 and 1.0, 0.0 meaning the tongue is at rest and
 * 	1.0 meaning the tongue is fully extended)
 */
void draw_frog(int x, int y, color_t color,
		float sf, float tongue_extension) {
	
	color_t lighter_shade =
	((color >> 11) + ((0x1F - (color >> 11)) / 3) << 11) |
	(((color & 0x7E0) >> 5) + ((0x3F - ((color & 0x7E0) >> 5)) / 3) << 5) |
	((color & 0x1F) + (0x1F - (color & 0x1F)) / 3);
	color_t darker_shade = (3*((color >> 11) / 4) << 11) |
		(3*(((color & 0x7E0) >> 5) / 4) << 5) |
		3*((color & 0x1F) / 4);
	
	// body
	draw_rect(x-100*sf,y-100*sf, 200*sf,200*sf, color);
 
	// left eye
	draw_rect((x-100*sf)-35*sf,(y-100*sf)-35*sf,
		70*sf,70*sf, lighter_shade);
	draw_rect((x-100*sf)-30*sf,(y-100*sf)-30*sf, 60*sf,60*sf, 0xFFFF);
	draw_rect((x-100*sf)-10*sf,(y-100*sf)-10*sf, 20*sf,20*sf, 0x0);
	
	// right eye
	draw_rect((x+100*sf)-35*sf,(y-100*sf)-35*sf,
		70*sf,70*sf, lighter_shade);
	draw_rect((x+100*sf)-30*sf,(y-100*sf)-30*sf, 60*sf,60*sf, 0xFFFF);
	draw_rect((x+100*sf)-10*sf,(y-100*sf)-10*sf, 20*sf,20*sf, 0x0);
	
	// mouth
	draw_rect(x-70*sf,y+30*sf, 140*sf,20*sf, 0x0);
	
	// left legs
	draw_rect((x-150*sf)-50*sf,(y+130*sf)-50*sf,
		100*sf,100*sf, darker_shade);
	draw_rect((x-40*sf)-10*sf,(y+130*sf)-50*sf,
		20*sf,100*sf, darker_shade);
	draw_rect((x-50*sf)-30*sf,(y+180*sf)-5*sf, 60*sf,10*sf, darker_shade);

	
	// right legs
	draw_rect((x+150*sf)-50*sf,(y+130*sf)-50*sf,
		100*sf,100*sf, darker_shade);
	draw_rect((x+40*sf)-10*sf,(y+130*sf)-50*sf,
		20*sf,100*sf, darker_shade);
	draw_rect((x+50*sf)-30*sf,(y+180*sf)-5*sf, 60*sf,10*sf, darker_shade); 
	
	// tongue
	draw_rect(x+30*sf,y+40*sf,
		30*sf,20*sf+100*sf*tongue_extension, 0xF165);
}

int main() {
	init_graphics();
	clear_screen();
	
	float tongue_extension = 0;
	int tongue_extending = 0;
	float frog_size = 0.5;
	color_t frog_color = 0x3DC6;
	int x = 300;
	int y = 200;
	
	char key = -1;
	while (key != 'q') {
		// background (disabled because it causes screen flickering)
		// I believe the screen flickering is due to simply too much
		// being drawn at once and causes a delay. Feel free to turn
		// it on. The flickering is not due to a timeout on select()
		// I double checked that.
		// draw_rect(0,0, 1000,500, 0x2C9F);
		
		// instructions	
		draw_text(5,5, "Welcome to the froggy's pond!", 0xFFFF);
		draw_text(5,25, "Use wasd to move, r and f to grow and "
			"shrink, and space to use your tongue.", 0xFFFF);
		draw_text(5,45, "Press q to quit.", 0xFFFF);
		
		draw_frog(x,y, frog_color, frog_size, tongue_extension);
				
		sleep_ms(100/3);
		
		key = getkey();
		if (key == 'w') { y -= 10; }
		else if (key == 'a') { x -= 10; }
		else if (key == 's') { y += 10; }
		else if (key == 'd') { x += 10; }
		else if (key == 'r') { frog_size += 0.1; }
		else if (key == 'f') { frog_size -= 0.1; }
		else if (key == ' ') { tongue_extending = 1; }
		
		if (frog_size <= 0.1) { frog_size = 0.1; }
		if (frog_size > 1) { frog_color = 0xC904; } // angry frog
		else { frog_color = 0x3DC6; } // normal frog
		
		if (tongue_extension >= 1) {
			tongue_extending = 0;
		} else if (tongue_extension < 0) {
			tongue_extension = 0;
		}
		
		if (tongue_extending) {
			tongue_extension += 0.1;
		} else if (!tongue_extending && tongue_extension > 0) {
			tongue_extension -= 0.1;
		}
		
		clear_screen();
		
	}
	exit_graphics();
}
