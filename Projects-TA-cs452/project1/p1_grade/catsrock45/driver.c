/**
* File : driver.c
* Author : Amin Sennour
* Purpose : This is a simple "move the square around" "game" which demonstrates 
*           the functionality provided by a graphics library implmenting 
*           graphics.h. 
*
* Gameplay : This "game" is a simple game which lets you move a square around 
*            a screen, while adjusting it's size and speed. 
*
* Special Features : 
*         Title : the game is titled "Basic Game". This demonstrates
*                 draw_text().
*      Controls : below the title some text explaining the controls is shown. 
*                 this demonstrates draw_text().
* Input Display : the last pressed key is drawn on the bottom left corner. This 
*                 demonstrates draw_text() and getkey(). 
*       Movment : you can move the square using (wasd). 
*                 This demonstrates getkey().
*        Growth : you can adjust the size of the square using (e/r). This 
*                 demonstrates getkey() and draw_rect().
*
* Remaining Functions : The remaining fuctions are demonstrated by the 
*                       operation of the program (Every iteration of draw_loop
*                       demonstrates sleep_ms() and clear_screen(), and this 
*                       program opening and closing cleanly demonstrates 
*                       init_graphics() and exit_graphics(). ).                      
*/


#include "graphics.h"
#include <stdio.h>


/**
 * Purpose : function to run the main draw loop of our program, redraw 
 *           the loop runs 100 times per second
 * Params : none
 * Return : void
 */
void draw_loop() {
    // define the colors to be used
    color_t blue = make_color(1,18,16);
    color_t orange = make_color(30,34,1);
    color_t green = make_color(6,63,0);

    // define a character to store the key pressed by the user
    char key;

    // define variables to track the position, size, and speed of the square
    int x = 0;
    int y = 0;
    int width = 50;
    int height = 50;
    int speed = 10;

    // store a single char string to print the last pressed key (blank) by 
    // default
    char last_key[1] = " ";

    // operate the draw loop
    do {
        // draw the ui
        draw_text(0, 0, "Basic Game", green);
        draw_text(0, 32, "Controls : ", green);
        draw_text(0, 48, "    move : wasd", green);
        draw_text(0, 64, "    grow : r", green);
        draw_text(0, 80, "  shrink : e", green);
        draw_text(0, 96, "   turbo : t", green);
        
        // handle user input
        key = getkey();
        switch (key) {
            case 'w':
                y = (y <= 0) ? y : y - speed;
                break;
            case 's':
                y = (y >= (480 - height)) ? y : y + speed;
                break;
            case 'a':
                x = (x <= 0) ? x : x - speed;
                break;
            case 'd':
                x = (x >= (640 - width)) ? x : x + speed;
                break;

            case 'r':
                width = (width >= 300) ? width : width + speed;
                height = (height >= 300) ? height : height + speed;
                break;
            case 'e':
                width = (width <= speed) ? width : width - speed;
                height = (height <= speed) ? height : height - speed;
                break;

            case 't':
                speed = (speed > 10) ? 10 : 50;
                break;

            default:
                break;
        }


        // if there is a new key being pressed update the last_key string
        if (key != '\0') {
            last_key[0] = key;
        }
        draw_text(0, (480 - 16), last_key, green);


        // The following logic draws the square in an alternating stripped 
        // pattern
        draw_rect(x, y, width, height, blue);
        // the ring algorithm only works if the width and the height are the 
        // same
        if (width == height) {
            // determine the numb of rings that can be drawn. 
            int rings = width / 10;
            // draw succeeding smaller rectangles atop one another to create
            // the ring effect
            int i;
            for (i = 0; i < rings; i++) {
                // determine how much to adjust the coordinates 
                int coord_adjust = i * 5;
                // determine how much to adjust the width / height 
                int width_adjust = i * 10;
                // select the alternating color 
                color_t c = (i % 2 == 0) ? blue : orange;
                // draw the rectange for this ring
                draw_rect(
                    x + coord_adjust, 
                    y + coord_adjust, 
                    width - width_adjust, 
                    height - width_adjust, 
                    c);
            }
        }

        // sleep before next frame
        sleep_ms(10);
        // clear to draw the next frame
        clear_screen();

    // if the key 'q' is pressed then exit the draw loop
    } while (key != 'q');    
}


/**
 * Purpose : entry point of the program
 */
int main(int argc, char const *argv[]) {
    // initialize our the graphics library 
    init_graphics();

    // operate the draw loop 
    draw_loop();

    // perform shutdown operations
    clear_screen();
    exit_graphics();
    return 0;
}