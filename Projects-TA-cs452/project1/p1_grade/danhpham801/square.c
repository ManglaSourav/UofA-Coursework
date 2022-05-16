typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);
color_t get_color_t(int r, int g, int b );
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
	int r = 0;
	int g = 0;
	int b = 31;
	color_t c = get_color_t(r,g,b);
	do
	{
		//draw a black rectangle to erase the old one
		draw_rect(x, y, 20, 20, 0);
		key = getkey();
		if(key == 'w') y-=10;
		else if(key == 's') y+=10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		//draw a blue rectangle
		draw_rect(x, y, 20, 20, c);
		sleep_ms(20);
	} while(key != 'q');
	exit_graphics();
	return 0;

}
