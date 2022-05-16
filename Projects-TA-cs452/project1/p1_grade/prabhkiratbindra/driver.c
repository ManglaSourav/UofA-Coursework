#include "library.h"

int x, y;

void welcome();

int main(void) {

    init_graphics();
    clear_screen();

    welcome();

    color_t color = 0b1111111111100000;                 // yellow

    char key;

    do {

        key = getkey();

        if (key == 10) {                                // ENTER

            y += 16;                                    // move down vertically
            x  = 16;                                    // reset horizontal position

        } else if (key == 27) {                         // ESC

            clear_screen();
            welcome();

        } else if ((key >= ' ') && (key <= '~')) {      // any printable character

            char str[2] = { key, '\0' };
            draw_text(x, y, str, color);
            x += 8;
        }

        sleep_ms(10);

    } while (key != 127);                               // while DEL is not pressed

    exit_graphics();

    return 0;
}

/**
 * prints the welcome text and instructions onto the screen
*/
void welcome() {

    x = 8;
    y = 6;

    color_t color = 0b1111100001100011;                 // slightly bright red

    char welcome1[] = "Welcome to \"Type For Fun\"!";
    draw_text(x, y, welcome1, color);

    y += 16;

    char welcome2[] = "Just type whatever you want and press 'BACKSPACE' when you are done!";
    draw_text(x, y, welcome2, color);

    y += 16;

    char welcome3[] = "If your text starts to overflow past the right side, just press 'ENTER'!";
    draw_text(x, y, welcome3, color);

    y += 16;

    char welcome4[] = "If you reach the bottom, press 'ESC' to clear your text area!";
    draw_text(x, y, welcome4, color);

    x +=  8;
    y += 32;
}
