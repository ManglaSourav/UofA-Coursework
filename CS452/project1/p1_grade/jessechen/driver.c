#include <stdint.h>
typedef uint16_t color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char* text, color_t c);

int main(int argc, char** argv) {
    int i = 0;
    init_graphics();
    char key;
    int maxX = 640;
    int maxY = 480;
    int x = 380;
    int y = (maxY - 20) / 2;

    int x2 = 50;
    int y2 = 100;

    draw_text(20, 20, "Three flashing rectangles.", 255);
    draw_text(300, 20, "Move this square with wasd keys.", 255);
    draw_text(300, 460, "Author: Jesse Chen, Copyright (C) 2022", 255);

    do {
        //draw a black rectangle to erase the old one
        draw_rect(x, y, 20, 20, 0);
        key = getkey();
        if (key == 'w') y -= 10;
        else if (key == 's') y += 10;
        else if (key == 'a') x -= 10;
        else if (key == 'd') x += 10;
        //draw a blue rectangle
        draw_rect(x, y, 20, 20, 255);

        // teleporting sq
        if (i == 0) {
            draw_rect(x2, y2, 60, 30, 0);
            draw_rect(x2, maxY - y2 - 30, 60, 30, 255);
            i = 1;
        } else if (i == 1) {
            draw_rect(x2, maxY - y2 - 30, 60, 30, 0);
            draw_rect(x2, maxY / 2 - 15, 60, 30, 255);
            i = 2;
        } else if (i == 2) {
            draw_rect(x2, maxY / 2 - 15, 60, 30, 0);
            draw_rect(x2, y2, 60, 30, 255);
            i = 0;
        }

        sleep_ms(20);
    } while (key != 'q');

    exit_graphics();

    return 0;

}
