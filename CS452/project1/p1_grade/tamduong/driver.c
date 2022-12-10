/**
 * File: driver.c
 * Author: Tam Duong
 * 
 * I make this driver for fun
 */

#include "library.h"

int main(void) {
    init_graphics();
    clear_screen();
    color_t allColor[] = {RGB(0,138,216), RGB(246,235,97), RGB(255,188,217)};
    int i = 0; 
    char key;
    draw_text(10, 300, "press q to quit, any key to surprise!", RGB(255, 255, 255));
    draw_rect(10, 10, 200, 200, RGB(255, 0, 0));
    draw_rect(20, 20, 200, 200, RGB(0, 255, 0));
    draw_rect(30, 30, 200, 200, RGB(0, 0, 255));
    do {
        key = getkey();
        draw_text(60, 60, "I love CSC452", allColor[i]);
        draw_text(70, 120, "Tam Duong", allColor[i]);
        i = (i + 1)%3;
    } while (key != 'q');
    

    exit_graphics();
}
