/*
* File: driver.c
* Author: Kaden Herr
* Date Created: Jan 2022
* Last editted: Feb 5, 2022
* Purpose: Custom driver file for library.c
*/

#include "graphics.h"

int main(int argc, char *argv[]) {
    char key;
    int moved = 0;
    int x = 640/2;
    int y = 480/2;
    int prev_x = x;

    int v_line_count = 0;
    char *cur_v_line;
    char v_line1[] = "I am a snowman!";
    char v_line2[] = "I can throw snowballs!";
    char v_line3[] = "Press 'F' to throw snowballs!";
    char snowball_line[] = "I lied, I can't throw snowballs!";

    // Set up the graphics and screen
    init_graphics();
    clear_screen();

    // Display text about how to move the snowman
    draw_text(x-88,20,"Press 'A' to move left",65535);
    draw_text(x-92,40,"Press 'D' to move right",65535);
    draw_text(x-88,60,"Press spacebar to talk",65535);
    draw_text(x-68,80,"Press 'Q' to quit",65535);

    // Draw the ground under the snowman
    draw_rect(0,362,639,2,65535);
    

    // Game loop
    do
    {
        // Draw a black rectangles to erase the old ones if they have moved
        if(moved) {
            draw_rect(prev_x,y,20,30,0);
            draw_rect(prev_x-20,y+30,60,90,0);
        }
        
        // Reset moved boolean variable
        moved = 0;

        // Get the action
        key = getkey();
        // Move left by 10px
        if(key == 'a') {
            prev_x = x;
            x-=10;
            moved = 1;
        } 
        // Move right by 10px
        else if(key == 'd') {
            prev_x = x;
            x+=10;
            moved = 1;
        } 
        // Play a voice line
        else if(key == ' ') {
            switch(v_line_count) {
                case 0:
                    cur_v_line = v_line1;
                    break;
                case 1:
                    cur_v_line = v_line2;
                    break;
                case 2:
                    cur_v_line = v_line3;
                    break;
            }
            v_line_count = ++v_line_count % 3;
            // Draw the text
            draw_text(x, y-18, cur_v_line, 2015);
            sleep_ms(2000);
            // Erase the text
            draw_text(x, y-18, cur_v_line, 0);

        // Play the snowball voice line
        } else if(key == 'f') {
            draw_text(x, y-18, snowball_line, 63680);
            sleep_ms(2000);
            // Erase the text
            draw_text(x, y-18, snowball_line, 0);
        }

        // Draw the snowman
        draw_rect(x,y,20,30,65535);
        draw_rect(x-20,y+30,60,90,65535);
        sleep_ms(20);

    } while(key != 'q');

    // Clean up
    clear_screen();
    exit_graphics();
}