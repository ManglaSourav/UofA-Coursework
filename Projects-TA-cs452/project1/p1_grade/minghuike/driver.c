#include "graphics.h"

/*
    Assigment:  CSC452 Project 1
    Author:     Minghui Ke
    Purpose:    Test the library.c function by a single game. 
                Display the text on the screen and user be able to
                use input interact with OS.
*/
int main(int argc, char** argv) {

    init_graphics();
    clear_screen();

    draw_text(100, 160, "Welcome!", RGB(5, 10, 15));
    draw_text(100, 180, "Please use letter to play with little rectangle.", RGB(5, 10, 15));
    draw_text(100, 200, "W: up, S: down, A: left, D: right.", RGB(20, 5, 10));
    draw_text(100, 220, "R: add red, G: add green, B: add blue.", RGB(20, 5, 10));
    draw_text(100, 240, "Press key to begin the game.", RGB(20, 5, 10));
    draw_text(100, 260, "Enjoy!", RGB(5, 10, 15));

    // Display the command line arguments, up to 8.
    int num = 1;
    int high = 320;
    while (num < argc) {
        draw_text(100, high, argv[num], RGB(20, 15, 10));
        high += 20;
        num++;
    }

    sleep_ms(5000);

    // ------------------begin the game--------------------//

    char key;
    int x = 480;
    int y = 420;
    int r = 15;
    int g = 10;
    int b = 5;

    do
    {
        // draw a black rectangle to erase the old one
        draw_rect(x, y, 40, 40, 0);
        key = getkey();

        if (key == 'w') y -= 20;
        else if (key == 's') y += 20;
        else if (key == 'a') x -= 20;
        else if (key == 'd') x += 20;
        else if (key == 'r') r += 1;
        else if (key == 'g') g += 1;
        else if (key == 'b') b += 1;

        // draw a rectangle
        draw_rect(x, y, 40, 40, RGB(r, g, b));
        sleep_ms(50);
    } while (key != 'q');
    
    clear_screen();
    exit_graphics();

    return 0;

}