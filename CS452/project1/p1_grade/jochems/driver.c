typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);

void draw_text(int x, int y, const char* text, color_t c);
int main(int argc, char** argv)
{
	int i;

	init_graphics();

	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;

	do
	{
		
		key = getkey();
		//draw a blue rectangle
		draw_rect(x, y, 20, 20, 25);
		draw_text(10,100,"HOWDY",25);
		sleep_ms(20);
	} while(key != 'q');

	exit_graphics();

	return 0;

}
