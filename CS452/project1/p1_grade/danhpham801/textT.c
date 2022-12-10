typedef unsigned short color_t;

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
color_t get_color_t(int r, int g, int b);
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t c);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

int main(int argc, char** args){
	char* s = "Hello World";
	int x = 0, y = 0, r=31,g=63,b=31;
	color_t c = get_color_t(r,g,b);
	init_graphics();
	clekar_screen();
	draw_text(x, y, s, c );
	exit_graphics();
	return 0;
}
