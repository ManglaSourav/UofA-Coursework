/**
 * @author Carter Boyd
 *
 * CSc_452, Spring 22
 *
 * This C program is designed to show the user how to test the library program
 * in order to show that all functions inside work. for the user there are
 * instructions when turning the program on that will be repeated here
 *
 * w: move up
 * d: move right
 * s: move down
 * a: move left
 * pressing any number will change the color of the square
 * : to have the uppercase and lowercase alphabet appear
 * > to clear the screen including the instructions
 * q: end process
 */

typedef unsigned short color_t;

void clear_screen();

void exit_graphics();

void init_graphics();

char getkey();

void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);

void draw_rect(int x1, int y1, int width, int height, color_t c);

void draw_text(int x, int y, char *text, color_t c);

int main() {
	init_graphics();
	char key;
	int x = (640 - 20) / 2;
	int y = (480 - 20) / 2;
	color_t color = 5;
	draw_text(1, 0, "Press a number to change the color, ':' to \
	show the alphabet, and '>' to clear the screen, w, s, d, a are the\
	keys to move", color);
	do {
		//draw a black rectangle to erase the old one
		key = getkey();
		draw_rect(x, y, 20, 20, 0);
		if (key == 'w') y -= 10;
		else if (key == 's') y += 10;
		else if (key == 'a') x -= 10;
		else if (key == 'd') x += 10;
		else if ( key == '1') color = 10;
		else if ( key == '2') color = 20;
		else if ( key == '3') color = 30;
		else if ( key == '4') color = 40;
		else if ( key == '5') color = 50;
		else if ( key == '6') color = 60;
		else if ( key == '7') color = 70;
		else if ( key == '8') color = 80;
		else if ( key == '9') color = 90;
		else if (key == '>') clear_screen();
		if (key == ':') {
			draw_text(x + 50, y, 
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ", color);
			draw_text(x + 50, y + 20, 
			"abcdefghijklmnopqrstuvwxyz", color);
		}
		draw_rect(x, y, 20, 20, color);
		sleep_ms(20);
	} while (key != 'q');
	exit_graphics();
	return 0;
}
