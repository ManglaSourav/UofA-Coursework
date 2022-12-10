typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);

int main(int argc, char** argv)
{
	int i;

	init_graphics();

	char key;
	
	int x1, x2, y1, y2, x3, y3;
	x1 = 150;
	x2 = 470;
	y1 = 100;
	y2 = 300;
	x3 = 170;
	y3 = 140;
	
	color_t white = 0xffff;
	color_t other = 0xff00;
	
	int oldy1, oldy2, oldy3, oldx3;
	
	draw_text(200, 200, "Press space to start", white);
	while (getkey() != 32) {}
	clear_screen();

	int flipped = 0;
	do {
		draw_rect(x1, oldy1, 20, 100, 0);
		draw_rect(x2, oldy2, 20, 100, 0);
		
		draw_rect(x1, y1, 20, 100, white);
		draw_rect(x2, y2, 20, 100, white);
		
		draw_rect(oldx3, oldy3, 20, 20, 0);
		draw_rect(x3, y3, 20, 20, other);
		if (y1 >= 300) {
			flipped = 1;
		}
		else if (y1 <= 100) {
			flipped = 0;
		}
		
		oldy1 = y1;
		oldy2 = y2;
		oldy3 = y3;
		oldx3 = x3; 
		if (!flipped) {
			y1 += 5;
			y2 -= 5;
			x3 += 7;
			y3 += 1;
		}
		else {
			y1 -= 5;
			y2 += 5;
			x3 -= 7;
			y3 -= 1;
		}
		sleep_ms(20);
	} while (getkey() != 'q'); 
	
	exit_graphics();

	return 0;

}
