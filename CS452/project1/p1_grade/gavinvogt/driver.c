/*
 * File: driver.c
 * Author: Gavin Vogt
 * Purpose: This program demonstrates all the required function
 * calls working. The drawing tools are a cyan square and a yellow
 * line of pixels, which can be toggled using Tab when in draw mode.
 */

#include "graphics.h"

#define DRAW_MODE 0
#define TEXT_MODE 1
#define RECT_TOOL 0
#define PIXEL_TOOL 1
#define MOVE_DIST 20

/*
 * Updates the text in the bottom right corner of the screen to
 * represent what mode it is in (text/draw)
 */
void updateModeText(int mode) {
    char *text;
    color_t color;
    if (mode == TEXT_MODE) {
        text = "[Text]";
        color = FROM_RGB(31, 0, 0);
    } else {
        text = "[Draw]";
        color = FROM_RGB(0, 63, 0);
    }
    draw_rect(592, 464, 48, 16, FROM_RGB(0, 0, 0));
    draw_text(592, 464, text, color);
}

/*
 * Processes a key read in during text mode.
 * Esc = switch to draw mode
 * Enter = move to new line
 * other = print that character
 */
int processKeyTextMode(char key, int *x, int *y) {
    if (key == 27) {
        // Esc = exit text mode
        updateModeText(DRAW_MODE);
        return DRAW_MODE;
    } else if (key == '\n') {
        *x = 0;
        *y = *y + 16;
    } else {
        // Print the key pressed
        char text[2] = {key, '\0'};
        draw_text(*x, *y, text, FROM_RGB(31, 0, 0));
        *x = *x + 8;
    }
    
    // Remain in text mode
    return TEXT_MODE;
}

/*
 * Draws from (x, y) in the given direction using the current draw tool
 */
void draw(int *x, int *y, int dirX, int dirY, int drawTool) {
    // Update the (x, y) location
    *x = *x + (dirX * MOVE_DIST);
    *y = *y + (dirY * MOVE_DIST);
    
    // Draw the new rect/line
    if (drawTool == RECT_TOOL) {
        draw_rect(*x, *y, MOVE_DIST, MOVE_DIST, FROM_RGB(0, 63, 31));
    } else {
        int i;
        int xPos = *x;
        int yPos = *y;
        color_t color = FROM_RGB(31, 63, 0);
        for (i = 0; i < MOVE_DIST; ++i) {
	    draw_pixel(xPos, yPos, color);
	    xPos -= dirX;
	    yPos -= dirY;
        }
    }
}

/*
 * Processes a keypress in draw mode
 * t = switch to text mode
 * wasd = move the tool
 * c = clear screen
 * Tab = toggle between rectangle/pixel tool
 */
int processKeyDrawMode(char key, int *x, int *y, int *drawTool) {
    if (key == 't' || key == 'T') {
        // Enter text mode
        updateModeText(TEXT_MODE);
        return TEXT_MODE;
    } else if (key == 'w' || key == 'W') {
        draw(x, y, 0, -1, *drawTool);
    } else if (key == 'a' || key == 'A') {
        draw(x, y, -1, 0, *drawTool);
    } else if (key == 's' || key == 'S') {
        draw(x, y, 0, 1, *drawTool);
    } else if (key == 'd' || key == 'D') {
        draw(x, y, 1, 0, *drawTool);
    } else if (key == 'c' || key == 'C') {
        // Clear screen and redraw the mode text
        clear_screen();
        updateModeText(DRAW_MODE);
    } else if (key == '\t') {
        if (*drawTool == RECT_TOOL) {
            *drawTool = PIXEL_TOOL;
        } else {
            *drawTool = RECT_TOOL;
        }
        draw(x, y, 0, 0, *drawTool);
    }
    
    // Remain out of text mode
    return DRAW_MODE;
}

/*
 * Starts up the graphics interface and provides an intro screen
 * displaying the keys to press
 */
void startInterface() {
    // Start the graphics
    init_graphics();
    clear_screen();
    
    // Draw welcome text
    char *hotkeys[] = {
        "w/a/s/d    Move draw tool",
        "  Tab      Switch draw tool",
        "   c       Clear drawing",
        "   t       Enter text mode",
        "  Esc      Exit text mode",
        "   q       Quit"
    };
    int i;
    color_t color = FROM_RGB(0, 45, 0);
    for (i = 0; i < 5; ++i) {
        draw_text(212, 200 + 16 * i, hotkeys[i], color);
    }
    draw_text(220, 320, "Press any key to continue", FROM_RGB(0, 63, 0));
    
    // Wait for any keypress to continue
    while (!getkey()) {
        sleep_ms(10);
    }
    clear_screen();
}

int main(int argc, char *argv[]) {
    // Start up the graphics interface
    startInterface();
    
    // Additional setup
    int mode = DRAW_MODE;
    int drawTool = RECT_TOOL;
    updateModeText(mode);
    char key;
    int x = 0;
    int y = 0;
    
    // Begin event loop
    draw(&x, &y, 0, 0, drawTool);
    do {
        key = getkey();
        if (key) {
            if (mode == TEXT_MODE) {
                // Text mode
                mode = processKeyTextMode(key, &x, &y);
            } else {
                // Not in text mode
                mode = processKeyDrawMode(key, &x, &y, &drawTool);
            }
        }
        sleep_ms(5);
    } while (!(mode == DRAW_MODE && (key == 'q' || key == 'Q')));
    
    // Clear and exit
    clear_screen();
    exit_graphics();
    return 0;
}
