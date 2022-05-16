/*
* File:    driver.c
* Date:    February 1, 2022
* Author:  Fernando Ruiz 
* Purpose: The following program uses an idea from square.c but with text and the
* trick that you can appear to erase the text on the screen.
*/


typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);

int main(int argc, char** argv)
{
	int i;
	init_graphics();
	char key;
	int x = (640-20)/2;
	int y = (480-20)/2;
	
	clear_screen();
	char str1[] = "HELLO WORLD!";
  	char str2[] = "ERASE ME BELOW";
    char str3[] = "Linux Sys calls rock!";
  	char str4[] = "ERASE ME ABOVE";
	draw_text(x+100, y-25,str1, 10000);
    sleep_ms(50);
    draw_text(x+100, y-50,str2, 10000);
    sleep_ms(50);
    draw_text(x-200, y-25, str4, 20000);
    sleep_ms(50);
    draw_text(x-200, y-50,str3, 20000);

	do
	{
		draw_rect(x, y, 35, 35, 0);
		key = getkey();
		if(key == 'w') y-=15;
		else if(key == 's') y+=15;
		else if(key == 'a') x-=15;
		else if(key == 'd') x+=15;
		draw_rect(x, y, 35, 35, 8000);
		sleep_ms(30);
	} while(key != 'q');
	clear_screen();
	exit_graphics();

	return 0;

}




