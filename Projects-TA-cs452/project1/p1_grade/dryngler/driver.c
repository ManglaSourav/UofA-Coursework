/**
 * @file driver.c
 * @author Daniel Ryngler
 * @brief Example program to show graphics library is performing properly
 * @version 0.1
 * @date 2022-02-06
 * 
 */

typedef unsigned short color_t;

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_pixel_square(int x, int y, int numPixels, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t color);


/**
 * @brief Example program to show graphics library is performing properly. 
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char** argv)
{
    init_graphics();

    char key;
    int x = (640-20)/2;
    int y = (480-20)/2;

    clear_screen();

    draw_text(10, 230, "WELCOME TO DRAWING", 31);
    draw_text(30, 260, "BY DANNY R.", 31);

    key = getkey();

    sleep_ms(5000);
    do
    {
        //clear the last drawing
        clear_screen();
        draw_text(10, 10, "USE KEYS w,s,a,d TO MOVE RECTANGLE", 31);

        //user input to move rectangle
        if(key == 'w') x-=10;
        else if(key == 's') x+=10;
        else if(key == 'a') y-=10;
        else if(key == 'd') y+=10;

        //draw a blue rectangle
        draw_rect(x, y, 50, 50, 25);

        sleep_ms(20); //sleep
        key  = getkey(); //get next key

    } while(key != 'q');

    exit_graphics();

    return 0;
}
