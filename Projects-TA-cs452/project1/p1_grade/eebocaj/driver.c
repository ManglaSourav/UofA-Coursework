/*
 * File: driver.c
 * Author: Jacob Edwards
 * This program allows the user to move a square around the screen,
 * as long as it stays within the red border.
 */
#include <stdio.h>
#include "colors.h"
#include "library.c"

void draw_border();
void draw_player(int centerx, int centery);

int main() {
	init_graphics();
	char key;
	int MOVEMENT_SPEED = 2;
	int playerX = 320;
	int playerY = 210;
	do {
		key = getkey();
		clear_screen();

		draw_border();
		draw_player(playerX, playerY);

		draw_text(20, 440, "Press (q) to exit.", YELLOW);

		if (key == 'w') {
			if (playerY > 40) playerY -= MOVEMENT_SPEED;
		} else if (key == 'a') {
			if (playerX > 40) playerX -= MOVEMENT_SPEED;
		} else if (key == 's') {
			if (playerY < 350) playerY += MOVEMENT_SPEED;
		} else if (key == 'd') {
			if (playerX < 570) playerX += MOVEMENT_SPEED;
		}
		update_frame(30);
	} while (key != 'q');

	clear_screen();
	update_frame(10);
	exit_graphics();
	return 0;
};

void draw_border() {
	draw_rectangle(20, 20, 600, 380, RED);
	draw_rectangle(25, 25, 590, 370, BLACK);
};

void draw_player(centerx, centery) {
	draw_rectangle(centerx - 20, centery - 20, 40, 40, GREEN);
}
