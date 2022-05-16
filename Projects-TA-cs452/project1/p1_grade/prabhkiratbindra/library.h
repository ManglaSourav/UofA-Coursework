typedef unsigned short color_t;     // 16-bit number; RGB - 5, 6, 5 bits

void init_graphics();
void exit_graphics();

void clear_screen();

char getkey();

void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char* text, color_t c);
