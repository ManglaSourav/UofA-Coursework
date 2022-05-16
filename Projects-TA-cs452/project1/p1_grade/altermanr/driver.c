// Driver file for the graphics library
// Written by Ryan Alterman
// Revised on 2/5/2022

#include "graphics.h"

#include <stdio.h>

int main(int argc, char** argv)
{
    // Initialize the graphics context
    init_graphics();

    int x = 0;
    int y = 0;
    int size = 20;

    char key = ' ';

    // Enter the game loop
    do
    {
        // Poll for user input
        key = getkey();

        if(key == 'w')
            y -= size;
        else if(key == 's')
            y += size;
        else if(key == 'a')
            x -= size;
        else if(key == 'd')
            x += size;

        // Make sure the image stays within the boundaries
        if(x >= 640)
            x = 0;
        if(x < 0)
            x = 640 - size;

        if(y >= 480)
            y = 0;
        if(y < 0)
            y = 480 - size;

        // Render
        clear_screen();
        draw_rect(x, y, size, size, convertRGB(0, 255, 125));
        draw_text(0, 0, "Ryan Alterman\n", convertRGB(255, 0, 0));

        sleep_ms(20);
    } while (key != 'q');

    // Close the graphics context
    exit_graphics();

    return 0;
}
