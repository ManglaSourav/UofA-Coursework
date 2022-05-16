/*
 * Author: Jesus Padres
 * Class: Csc 452
 * Instructor: Jonathan Misurda
 * Due: Feb 6 2022
 *
 * Purpose: Uses library.c to create a fully functional
 * notepad using the terminal framebuffer.
 *
 */

// Draws notepad
void draw_notepad() {
 	draw_rect(100, 10, 440, 45, 0xFF80);
 	draw_rect(100, 55, 440, 425, 0xFFFE);
 	draw_text(290, 26, "Notepad", 0);

 	int x = 110;
 	int y = 80;

 	while (y < 480) {
 		draw_rect(x, y, 420, 1, 0);

 		y += 20;
 	}
 }

int main(int argc, char** argv) {
	init_graphics();
	char key;

	clear_screen();
	draw_text(2, 2, "Press 'esc'", 0xFFFF);
	draw_text(2, 18, "to exit", 0xFFFF);

	draw_notepad();

	int x = 110;
	int y = 64;

	// Draws and keeps track of keys typed
	do
	{
		draw_rect(x, y, 2, 16, 0);

		key = getkey();

		sleep_ms(50);
		draw_rect(x, y, 2, 16, 0xFFFE);
		if (key == 8 || key == 127) {
			if (x > 110) {
				x -= 8;
				draw_rect(x, y, 8, 16, 0xFFFE);
			} else if (y > 64) {
				x = 526;
				y -= 20;
			}
		} else if (y < 444) {
			draw_letter(x, y, key, 0);
			x += 8;
			if (x > 518 || key == '\n') {
				x = 110;
				y += 20;
			}
		}
	} while(key != 27);

	exit_graphics();
	clear_screen();
	return 0;
}
