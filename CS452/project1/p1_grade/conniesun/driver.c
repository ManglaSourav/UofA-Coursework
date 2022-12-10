#include "graphics.h"

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
/*
 * ***note*** driver.C uses standard library
 * for calling rand(). library.C does not use 
 * the C standard library! (Misurda said OK)
 */
#include <stdlib.h>

/*
 * driver.c
 *
 * Author: Connie Sun
 * Course: CSC 452 Spring 2022
 * 
 * A driver file to show that the graphics library file
 * (library.C) works correctly. Implements a very basic
 * snake-like game.
 * 
 * The snake moves one block at a time in the 'wasd' directions.
 * The objective is to eat apples (red rectangles) that appear
 * at random locations on the screen. Note that the snake is
 * allowed to fold back in on itself, and the user only loses
 * if the snake goes out of bounds of the screen. Theoretically,
 * the game can be played until the snake fills the entire screen.
 *
 */

// define some constants used in the game
#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4
#define SIZE 20
#define WIDTH 640
#define HEIGHT 480

// each square of the snake's body is a
// loc; they form a doubly linked list
// the apple is also a loc
typedef struct loc {
    int x;
    int y;
    struct loc *next;
    struct loc *prev;
} loc;

// global vars
struct loc *head;
struct loc *tail;
struct loc *apple;
int length;

/*
 * generate random locations for the apple and update
 * its location in the loc struct. all x and y locations
 * should be multiples of 20 for alignment.
 */
void random_apple() {
    unsigned int tmp;
    int x = apple->x;
    int y = apple->y;
    while (is_snake(x, y)){
    	tmp = rand();
    	x = tmp % (WIDTH/SIZE - 1);
    	tmp = rand();
    	y = tmp % (HEIGHT/SIZE - 1);
    }
    apple->x = x * SIZE;
    apple->y = y * SIZE;
}

/*
 * draw the apple to the screen by calling draw_rect()
 */
void draw_apple() {
    draw_rect(apple->x, apple->y, SIZE, SIZE, RED);
}
/*
 * checks if the given location is part of the snake's 
 * body. returns 1 if yes, 0 otherwise.
 */
int is_snake(int x, int y) {
    struct loc *cur = tail;
    while (cur){
    	if (cur->x == x && cur->y == y)
   	    return 1;
   	cur = cur->next;
    }
    return 0;    
}

/*
 * checks if the given location is the snake's head. used
 * to determine if the head ran into another body loc.
 * returns 1 if yes, 0 otherwise.
 */
int is_head(int x, int y){
    if (x == head->x && y == head->y)
    	return 1;
    return 0;
}
/*
 *
 */
int is_prev(int x, int y){
    if (head->prev){
    	if (head->prev->x == x && head->prev->y == y)
    	    return 1;
    }
    return 0;
}

/*
 * this function should be called if the user moved the 
 * head of the snake over the apple.
 * 
 * all the logic for when an apple is eaten and the snake grows.
 * 
 * calculates the location of the new tail using the direction
 * of the current tail's next body loc (e.g., if the tail is left
 * of its next loc, the new tail will grow to the left). default
 * is to grow to the left for a snake of length 1.
 * 
 * increments the length, updates the linked list, and draws the
 * new tail. also calls random_apple() and draw_apple() to generate
 * a new apple and draw it to the screen.
 */
void eat_apple() {
    struct loc *new_tail;
    new_tail = (loc *) malloc(sizeof(loc));

    // which direction to grow
    int x = tail->x;
    int y = tail->y;
    if (length > 1) {
        x += x - tail->next->x;
        y += y - tail->next->y;
    }
    if (length == 1)
        x -= 20;
    // update linked list
    new_tail->x = x;
    new_tail->y = y;
    new_tail->next = tail;
    new_tail->prev = NULL;
    tail->prev = new_tail;

    tail = new_tail;
    draw_rect(tail->x, tail->y, SIZE, SIZE, GREEN);

    length ++;
    // new apple
    random_apple();
    draw_apple();
}

/*
 * set up for the snake game. initializes a head and tail of 
 * a snake of length 1 (the same loc) and draws it in a pre-
 * determined location. also draws the first apple, also pre-
 * determined.
 */
void start_game(int x, int y) {
    // initialize snake game
    head = (loc *) malloc(sizeof(loc));
    tail = (loc *) malloc(sizeof(loc));

    head->x = x;
    head->y = y;
    head->next = NULL;
    head->prev = NULL;

    tail = head;

    length = 1;
    draw_rect(head->x, head->y, SIZE, SIZE, GREEN);
    // need to create the initial apple and draw
    apple = (loc *) malloc(sizeof(loc));
    apple->x = x + SIZE * 8;
    apple->y = y;

    draw_apple();
}

/*
 * this function should be called when the user enters a
 * valid direction using the 'wasd' keys. 
 * 
 * creates a new head location based on the direction moved.
 * to draw the snake moving, a black rectangle is drawn over 
 * the old tail and the tail is set to its next pointer (if 
 * length > 1). a new rectangle is drawn where the new head is.
 * if the snake moves onto the apple, eat_apple() is called.
 * 
 * returns 1 if the move was unsuccessful (user moved the snake
 * out of bounds of the screen). returns 0 otherwise.
 */
int move_snake(int dir) {
    // new head to draw
    int new_x = head->x;
    int new_y = head->y;
    if (dir == LEFT) new_x -= SIZE;
    else if (dir == RIGHT) new_x += SIZE;
    else if (dir == UP) new_y -= SIZE;
    else if (dir == DOWN) new_y += SIZE;
    // check out of bounds
    if (new_x < 0 || new_x >= WIDTH - 1 ||
        new_y < 0 || new_y >= HEIGHT - 1)
        return 1;
    // check hits itself
    if (is_snake(new_x, new_y) && !is_head(new_x, new_y))
    	return 1;
    // draw black rectangle over the tail
    draw_rect(tail->x, tail->y, SIZE, SIZE, BLACK);
    // move tail up
    if (length > 1)
        tail = tail->next;
    
    struct loc *new_head;
    new_head = (loc *) malloc(sizeof(loc));
    // update the linked list
    new_head->x = new_x;
    new_head->y = new_y;
    new_head->next = NULL;
    new_head->prev = head;
    head->next = new_head;

    head = new_head;
    if (length == 1)
        tail = head;
    draw_rect(head->x, head->y, SIZE, SIZE, GREEN);
    // ate an apple
    if (head->x == apple->x && head->y == apple->y)
        eat_apple();
    return 0;
}

/*
 * the main play function that does the set-up logic for
 * playing the game. clears the screen from the intro or
 * end game display and intializes the game. continually
 * gets the keypress from the user to move the snake. when
 * the user loses (out of bounds), draws the end game 
 * display. the score is drawn as the # of apples the user
 * ate with time between each apple.
 */
void play_snake() {
    clear_screen();

    char key;
    int x = SIZE*8;
	int y = SIZE*10;
    int done, dir;
    start_game(x, y);
	do
	{
		key = getkey();
		if(key == 'w') {
		    if (dir != DOWN)
			dir = UP;
		}
		else if(key == 's') {
		    if (dir != UP)
			dir = DOWN;
		}
		else if(key == 'a') {
		    if (dir != RIGHT)
			dir = LEFT;
		}
		else if(key == 'd') {
		    if (dir != LEFT)
			dir = RIGHT;
		}
        	done = move_snake(dir);
        	sleep_ms(100);
	} while(done != 1);

    clear_screen();
    draw_text(10*SIZE, SIZE * 2, "GAME OVER :(", MAGENTA);
    draw_text(10*SIZE, SIZE * 3, "SCORE:", WHITE);
    int i;
    int score_x = 240;
    int score_y = SIZE*3;
    for (i = 0; i < length - 1; i++) {
        if (i % 5 == 0) {
            score_y += 30;
            score_x = 240;
        }
        score_x += 10;
        draw_rect(score_x, score_y, SIZE, SIZE, RED);
        score_x += 20;
        sleep_ms(300);
    }
    // exit stuff
}

/*
 * sets up the graphics device. introduces the user to the snake 
 * game by giving directions. when the user finishes a game, allows
 * the user to either play again or quit. on quit, calls exit to 
 * clean up.
 */
int main(int argc, char** argv)
{
    srand(time(0)); // random seed, for the apples
    char key;

    // setup stuff
	init_graphics();
    int text_x = WIDTH/2 - SIZE*2;
    int text_y = SIZE * 2;
    // letters are width 8
    draw_rect(text_x - 20, text_y - 4, 110, 24, GREEN);
	draw_text(text_x, text_y, "bad snake", BLACK);

    draw_text(text_x, text_y + SIZE*2, "wasd to move",MAGENTA);
    draw_text(text_x, text_y + SIZE*5, "s to start", WHITE);
    // wait for user to start
    do {
        key = getkey();
    } while (key != 's');
    // allow user to play again or quit
    char play_again = 'r';
    while (play_again != 'q') {
    	if (play_again == 'r')
        	play_snake();
        draw_text(text_x - SIZE*4, HEIGHT - SIZE * 5, "r to play again, q to quit", GREEN);
        play_again = getkey();
    }

	exit_graphics();

	return 0;

}
