typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

int main(int argc, char** argv) {

	init_graphics();

	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;
	int rgb = 0;
	color_t c = 0xFFFF;

	do
	{
		draw_text(0, 0, "W/A/S/D to move", 0xFFFF);
		draw_text(0, 16, "C to toggle RGB", 0xFFFF);
		draw_text(0, 32, "Q to toggle quit", 0xFFFF);
		//draw a black rectangle to erase the old one
		draw_rect(x, y, 30, 30, 0);
		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		else if(key == 'c') rgb = ~rgb;
		if(rgb != 0) c += 1;
		draw_rect(x, y, 20, 20, c);
		draw_rect(x + 5, y + 5, 10, 10, 0);
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();
	return 0;
}
