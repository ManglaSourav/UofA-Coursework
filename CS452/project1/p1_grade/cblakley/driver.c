/* Author: Cole Blakley
   Description: A simplified version of Wordle, a word game where you
     try to guess a 5 letter word in six guesses. All functionality
     from library.c is used:
       - draw_pixel/draw_rect to create the title header
       - draw_text to draw to guesses/instructions
       - color_t to highlight text/guesses
       - initialize_graphics/clear_screen/exit_graphics to setup/cleaup
         the screen
       - getkey to read keystrokes for guesses
       - sleep_ms to wait for user input
*/
#include "graphics.h"

// The number of guesses
#define GUESS_COUNT 6
// The number of characters in a guess
#define GUESS_LEN 5

color_t green;
color_t white;
color_t blue;

// Print each element of lines on its own line, with the top
// left corner of line 0 being (x, y).
void draw_multiline_text(int x, int y, color_t color,
                         const char* lines[], int line_count)
{
    int i;
    for(i = 0; i < line_count; ++i) {
        draw_text(x, y, lines[i], color);
        y += FONT_HEIGHT;
    }
}

int str_len(const char* text)
{
    int len = 0;
    const char* curr_char = text;
    while(*curr_char++ != '\0') {
        ++len;
    }
    return len;
}

// Print a message and wait for user input, which is printed
// as the user types and written to guess_out. Returns the
// x-coordinate to the right of the prompt message.
int prompt(int x, int y, color_t color, const char* msg,
           char* guess_out, int guess_len)
{
    draw_text(x, y, msg, color);
    int guess_x = str_len(msg) * FONT_WIDTH + FONT_WIDTH;
    int i = 0;
    while(i < guess_len) {
        char curr_char = getkey();
        if(curr_char != 0) {
            // Append new letter to end of guess
            guess_out[i++] = curr_char;
            guess_out[i] = '\0';
            draw_text(guess_x, y, guess_out, white);
        }
        sleep_ms(10);
    }
    return guess_x;
}

// Determines what to color a given letter based on the answer.
color_t get_letter_color(const char* answer, char letter, int pos)
{
    if(letter == answer[pos]) {
        return green;
    } else {
        int i;
        for(i = 0; i < GUESS_LEN; ++i) {
            if(answer[i] == letter) {
                return blue;
            }
        }
        return white;
    }
}

// Draws a 1px thick horizonal line starting at (x, y).
void draw_line(int x, int y, int len, color_t color)
{
    int i;
    for(i = 0; i < len; ++i) {
        draw_pixel(x + i, y, color);
    }
}

const char* rules[] = {
    "How to play:",
    "- Guess the 5 letter word. You have 6 guesses.",
    "- Letters that are in the right place will be marked green",
    "- Letters that are in the wrong place will be marked blue.",
    "- Letters that are not in the word will be white."
};

// Draws the rules and the Wordle header
void draw_header()
{
    draw_rect(SCREEN_WIDTH / 2, 0, FONT_WIDTH*6, FONT_HEIGHT, blue);
    draw_text(SCREEN_WIDTH / 2, 0, "WORDLE", green);
    draw_line(0, FONT_HEIGHT + 2, SCREEN_WIDTH, green);
    draw_multiline_text(0, FONT_HEIGHT + 5, white, rules,
                        sizeof(rules) / sizeof(rules[0]));
}

char guesses[GUESS_COUNT][GUESS_LEN];

const char* answer = "bread";

int main()
{
    green = make_color(0, 63, 5);
    white = make_color(31, 63, 31);
    blue = make_color(0, 0, 31);
    init_graphics();
    clear_screen();
    draw_header();

    int i;
    int y = 100;
    for(i = 0; i < GUESS_COUNT; ++i) {
        int guess_x = prompt(0, y, green, "Enter guess: ", guesses[i],
                             GUESS_LEN);
        int correct_letters = 0;
        int j;
        for(j = 0; j < GUESS_LEN; ++j) {
            color_t color = get_letter_color(answer, guesses[i][j], j);
            draw_char(guess_x + j*FONT_WIDTH, y, guesses[i][j], color);
            if(color == green)
                ++correct_letters;
        }
        if(correct_letters == GUESS_LEN) {
            draw_text(0, 200, "You win!", green);
            break;
        }
        y += FONT_HEIGHT;
    }

    draw_text(0, 220, "Press 'q' to exit.", white);
    char c;
    while((c = getkey()) != 'q') {
        sleep_ms(10);
    }
    clear_screen();
    exit_graphics();

    return 0;
}
