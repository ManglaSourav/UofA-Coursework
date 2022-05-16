/**
 * Author:  Mark Oakeson
 * Class: CSc 452
 * Instructor: Misurda
 * Project:  1
 * File: driver.c
 *
 * Description: Program is used to show basic functionality of the graphics library created in
 * library.c
 *
 * NOTE: Program allows user to color outside of border
 */

#include "graphics.h"

int main(){
    char key;
    init_graphics();

    draw_rect(0, 0, 639, 479, 65535); //Make one big white rectangle to cover the entire canvas

    int curX = 0;
    int curY = 0;

    // While loops create a red border around the canvas
    while(curX < 639) {
        draw_rect(curX, 0, 20, 20, 60000);
        draw_rect(curX, 460, 20, 20, 60000);
        curX += 20;
    }
    while(curY < 460) {
        draw_rect(0, curY, 20, 20, 60000);
        draw_rect(620, curY, 20, 20, 60000);
        curY += 20;
    }

    draw_text(200, 5, "Use keys A, S, D, W to color!", 0); // Text in top border

    int x = (640 - 20) / 2;
    int y = (480 - 20) / 2;
    // used to start in center of screen
    do {

        key = getkey();

        if(key == 'w') y-=10;
        else if(key == 's') y+=10;
        else if(key == 'a') x-=10;
        else if(key == 'd') x+=10;

        draw_rect(x, y, 20, 20, 0);

        sleep_ms(20);
    }while (key !='q');
    exit_graphics();
    return 0;
}


