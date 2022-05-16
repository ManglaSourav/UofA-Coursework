/*
 * File: driver.c
 * Author: Justin Duross
 * Purpose: This program acts as a driver for my graphics
 * library. It is a game of Simon, where an ever increasing sequence of
 * colors is displayed on the screen and the player has to input the
 * sequence themselves. If they get 10 points then they win and the game
 * is over. If they input the wrong sequence then they lose and the game
 * is over. stdlib.h is included only for rand() calls.
*/
#include <stdlib.h>

typedef unsigned short color_t;

void init_graphics();
void exit_graphics();
void clear_screen();
char get_key();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

/*
 * This function prints the welcome message and instructions for the
 * game. This is the first thing the player sees.
*/
void print_welcome_message() {
	draw_text(640 / 2 - 75, 480 / 4, "Welcome to Simon!", 0xFFFF);
	sleep_ms(2000);
	draw_text(640 / 2 - 225, 480 / 3 + 20, "There will be a\
	sequence of colors displayed on the screen.", 0xFFFF);
	sleep_ms(2000);
	draw_text(640 / 2 - 125, 480 / 3 + 40, "When the sequence is\
	done,",  0xFFFF);
	sleep_ms(1500);
	draw_text(640 / 2 - 225, 480 / 3 + 60, "you will input the\
	sequence yourself in the correct order.", 0xFFFF);
	sleep_ms(2000);
	draw_text(640 / 2 - 175, 480 / 3 + 80, "The sequences will get\
	longer each round.", 0xFFFF);
	sleep_ms(2000);
	draw_text(640 / 2 - 200, 480 / 3 + 100, "You get a point for\
	each sequence you get correct.", 0xFFFF);
	sleep_ms(2000);
	draw_text(640 / 2 - 155, 480 / 3 + 120, "You need 10 points to\
	win the game.", 0xFFFF);
	sleep_ms(2000);
	draw_text(640 / 2 - 100, 480 / 3 + 200, "Press ENTER to begin", 
	0xFFFF);
}

/*
 * Function for drawing the green square to the screen, it is always in
 * the same location.
*/
void draw_green() {
	draw_rect(135, 40, 175, 175, 0x07E0);
}

/*
 * Draws the red square to the screen, always in the same location.
*/
void draw_red() {
	draw_rect(330, 40, 175, 175, 0xF800);
}

/*
 * Draws the yellow square to the screen, always in the same location.
*/
void draw_yellow() {
	draw_rect(135, 235, 175, 175, 0xFFE0);
}

/*
 * Draws the blue square to the screen, always in the same location.
*/
void draw_blue() {
	draw_rect(330, 235, 175, 175, 0x001F);
}

/*
 * Generates the length 10 sequence of colors to display. Uses random
 * number generators and modulo to pick between the four different
 * colors. The same color is never back to back in a sequence.
*/
void generate_sequence(int round, int sequence[]) {
	int i;
	int randomint;
	int prev_randomint = -1;
	for (i = 0; i < 14; i++) {
		do {
			randomint = rand() % 4;
		} while (prev_randomint == randomint);
		prev_randomint = randomint;
		sequence[i] = randomint;
	}
}

/*
 * Iterates through the sequence depending on the current round. The
 * first round displays 4 colors in the sequence, then every round adds
 * one more. Calls the correct color square to the screen depending on
 * the random int.
*/
void show_sequence(int round, int sequence[]) {
	int i;
	for (i = 0; i < round + 4; i++) {
		draw_rect(50, 40, 505, 440, 0);
		sleep_ms(20);
		if (sequence[i] == 0) {
			draw_green();
		}
		else if (sequence[i] == 1) {
			draw_red();
		}
		else if (sequence[i] == 2) {
			draw_yellow();
		}
		else if (sequence[i] == 3) {
			draw_blue();
		}
		sleep_ms(750);
	}
}

/*
 * Displays all of the colored squares and the proper keys to press for
 * each color. It will then get the key that was pressed and if it was
 * correct then it will listen for more until it has reached the number
 * of colors that were displayed. If the player gets the keypresses
 * correct it will return 0, if not it will return 1.
*/
int guess_sequence(int round, int sequence[]) {
	draw_green();
	draw_red();
	draw_yellow();
	draw_blue();
	draw_text(120, 440, "Q for GREEN", 0x07E0);
	draw_text(240, 440, "W for RED", 0xF800);
	draw_text(340, 440, "A for YELLOW", 0xFFE0);
	draw_text(460, 440, "S for BLUE", 0x001F);
	
	char key;
	int i = 0;
	while (i < round + 4) {
		key = getkey();
		if (sequence[i] == 0 && key == 'q') {
			i++;
		}
		else if (sequence[i] == 1 && key == 'w')
		{
			i++;
		}
		else if (sequence[i] == 2 && key == 'a') {
			i++;
		}
		else if (sequence[i] == 3 && key == 's') {
			i++;
		}
		else if (key != '\0') {
			return 1;
		}
	}
	return 0;
}

/*
 * Displays a simple win screen once the players get 10 points.
*/
void show_win_screen() {
	clear_screen();
	draw_text(640/2 - 75, 480/2 - 100, "Congratulations!", 0xFFFF);
	draw_text(640/2 - 50, 480/2 - 50, "You win!", 0xFFFF);
	draw_text(640/2 - 100, 480/2, "You got all 10 points!", 0xFFFF);
}

/*
 * Displays a simple game over screen if the player gets the sequence
 * incorrect.
*/
void show_game_over() {
	clear_screen();
	draw_text(640/2 - 50, 480/2, "Game over!", 0xFFFF);
	draw_text(640/2 - 100, 480/2 + 25, "You input the sequence\
	incorrectly", 0xFFFF);
}

/*
 * Contains the basic game loop. It first calls generate_sequence() for
 * the sequence of colors to use throughout. Displays the nunmber of
 * points the user currently has. Then calls show_sequence() and finally
 * guess_sequnce(). If guess_sequence returns a game over then the loop
 * will exit and display the game over screen. If the player gets all 10
 * points then it will exit the loop and display the win screen.
*/
void start_game() {
	int round = 0;
	char points[] = "0";
	draw_text(300, 10, "Points: ", 0xFFFF);
	int gameover = 0;
	int sequence[14];
	generate_sequence(round, sequence);
	while (!gameover && round < 10) {
		draw_text(360, 10, points, 0xFFFF);
		show_sequence(round, sequence);
		gameover = guess_sequence(round, sequence);
		if (gameover) {
			break;
		}
		round++;
		draw_text(360, 10, points, 0);
		points[0]++;
	}
	if (!gameover) {
		show_win_screen();
	}
	else {
		show_game_over();
	}
}

int main() {
	srand(time(NULL));
	clear_screen();
	init_graphics();

	print_welcome_message();

	char key = getkey();
	while (getkey() != 0x0a) {

	}
	clear_screen();

	start_game();

	exit_graphics();
	return 0;
}
