/*
 * File: driver.c
 * Author: Xinyi Shao
 */

#include "graphics.h"

int main() {

    init_graphics();

    // Test draw_pixel
    draw_pixel(50, 50, RGB(31, 0, 0));
    draw_pixel(150, 150, RGB(31, 0, 0));

    // Test draw_rect
    draw_rect(50, 250, 50, 100, RGB(0, 31, 0));
    draw_rect(200, 250, 100, 50, RGB(0, 31, 0));

    // Test draw_char
    draw_char(100, 100,'B', RGB(0,0,31));

    // Test draw_text
    draw_text(200, 50,"!\"#$%&'( )*+,-./0123456789:;<=>?@`[]{}~\\^_|", RGB(0,0,31));
    draw_text(200, 70,"ABCDEFGHIJKLMNOPQRSTUVWXYZ", RGB(0,0,31));
    draw_text(200, 90,"abcdefghijklmnopqrstuvwxyz", RGB(0,0,31));

    // Test getKey and clear_screen
    // q for quit, c for clear screen
    char key;
    do {
        key = getkey();
        if (key == 'c') {
            clear_screen();
        }
        sleep_ms(200);
    } while (key != 'q');

    exit_graphics();

    return 0;
}

