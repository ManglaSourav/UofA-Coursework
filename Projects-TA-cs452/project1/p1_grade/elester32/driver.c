/*
	author: Oliver Lester
	description: This file is the additional driver that shows all
	functions in library.c are working. It will do so in the form of
	a game. It will have options to play or exit. Ask for players
	name. And provides a mini game.

	Name:
		Have 6 space. Will continue after 6 characters are
		entered, or enter is placed. Can backspace, and works
		good.

	Controls:
		standard WASD controls.
		press 'q' to quit while in game.
		reach goal to win (will exit game).
*/

#include "graphics.h"

/*
	This function sets up the game for the player.
*/
int main() {

	init_graphics();
	clear_screen();
	draw_text(296,120,"Welcome", 65535);
	sleep_ms(2500);
	clear_screen();
	sleep_ms(2500);

	char st;
	int con = 0;

	do
	{
		draw_text(224, 150, "What would you like to do", 65535);
		draw_text(268, 182, "->", 65535);
		draw_text(300, 182, "0 : Play", 65535);
		draw_text(300, 214, "1 : Exit", 65535);

		if (st == 49) {
			erase(268, 182, 32, 32);
			draw_text(268, 214, "->", 65535);
			con = 1;
		}
		if (st == 48) {
			erase(268, 214, 32, 32);
			draw_text(268, 182, "->", 65535);
			con = 0;
		}

		st = getkey();

	} while(st!=10);

	if (con == 0) {
		name();
	}

	clear_screen();
	exit_graphics();

	return 0;
}

/*
	This function gets a name from the user. The name is at most 6
	letters long.
*/
void name() {
	char player[6] = "      ";
	clear_screen();
	draw_text(192, 150, "Enter Name - Up to 6 characters", 65535);

	int i = 0;
	int x = 296;
	int y = 182;
	char th;

	while (i < 6) {
		th = getkey();
		if (th == 10) {
			break;
		} else if (th == 127) {
			if (i > 0) {
				player[i] = ' ';
				x = x - 8;
				erase(x, y, 8, 16);
				i--;
			}
		} else {
			draw_letter(x, y, th, 65535);
			player[i] = th;
			i++;
			x = x + 8;
		}
	}

	game(player, i);
}

/*
	The actual contents of the game are held here.
*/
void game(char* temp, int s) {
	clear_screen();

	char name[6] = "      ";
	int i;

	for (i = 0; i < 6; i++) {
		name[i] = temp[i];
	}

	int ch;
	int col_1 = 405;
	int col_2 = 305;
	int col_3 = 430;
	int col_4 = 330;

	int x = 110;
	int y = 410;

	do
	{
	draw_text(272, 75, "Hello ", 65535);
	draw_text(320, 75, name, 65535);
	draw_rect(100, 125, 440, 305, 65535);

	draw_rect(305, 405, 25, 25, 65535);
	draw_rect(520, 400, 0, 30, 60000);
	draw_rect(x, y, 20, 20, 1000);

	ch = getkey();

	if (ch == 'a') {
		draw_rect(x, y, 20, 20, 0);
		if (y < 410) {
			y = y + 5;
			if (x>285 && x<330 && y == 390) {
				y = y - 5;
			}
		}
		if (x != 330 || y + 20 < 405) {
			x = x - 5;
			if (x < 100) {
				x = x + 5;
			}
		}
	}

	if (ch == 'd') {
		draw_rect(x, y, 20, 20, 0);
		if (y < 410) {
			y = y + 5;
			if (x>285 && x<330 && y == 390) {
				y = y - 5;
			}
		}
		draw_rect(x, y, 20, 20, 0);
		if (x + 20 == 525) {
			draw_text(264, 100, "Victory", 65535);
			ch = 'q';
			sleep_ms(2500);
		}
		if (x + 20 != 305 || y + 20 < 405) {
			x = x + 5;
		}
	}

	if (ch == 'w') {
		if (y == 410) {
			draw_rect(x, y, 20, 20, 0);
			int i = 0;
			while (i < 10) {
				draw_rect(x,y,20,20,1000);
				sleep_ms(50);
				draw_rect(x, y, 20, 20, 0);
				y = y - 5;
				i++;
			}
		draw_rect(x, y, 20, 20, 0);
		}
	}

	} while(ch!='q');
}
