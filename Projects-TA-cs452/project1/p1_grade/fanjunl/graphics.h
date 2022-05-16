typedef unsigned short color_t;
char getkey();
void draw_pixel(int x, int y, color_t color);
int isOver(int x, int y);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void exit_graphics();
void clear_screen();
void sleep_ms(long ms);
void draw_char(int x, int y, char c, color_t co);
void draw_text(int x, int y, const char* text, color_t co);
void init_graphics();