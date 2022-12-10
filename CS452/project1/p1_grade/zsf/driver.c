
typedef unsigned short color_t; 

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

int main() {
  
     init_graphics(); 

     int x = 50; 
     int y = 50; 
     char key; 

     do {
	     draw_rect(x, y, 20, 20, 0);
	     key = getkey();
	     if (key == 'w') y -= 10; 
	     else if (key == 's') y += 10; 
	     else if (key == 'a') x -= 10; 
	     else if (key == 'd') x += 10;
	     else if (key == 'c') {
		     do {
			     clear_screen(); 
			     key = getkey();
		     } while (key != 'c');
	     } 
	     draw_rect(x, y, 20, 20, 32768);
	     sleep_ms(20); 
     } while (key != 'q');


    // Before we exit our program we want to clean up everything. 
    exit_graphics();

    return 0; 
}
