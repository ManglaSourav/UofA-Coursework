/*
 * Author: Molly Opheim
 * Class: CSc 452
 * File: graphics.h
 * This header file, graphics.h, provides typedefs and function 
 * declarations necessary for 
 * the library functions that implement graphics in library.c
 */
#include <sys/stat.h>
#include <sys/mman.h>
#include <stddef.h>
#include <linux/fb.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
typedef unsigned short color_t;
void init_graphics();
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);
void draw_char(int x, int y, char letter, color_t c);
void sleep_ms(long ms);
void exit_graphics();
void clear_screen();
char getkey();
void *mPtr;
int mmapSize;
int fileDes;
struct fb_var_screeninfo vscreen;
struct fb_fix_screeninfo fscreen;
struct termios cur_t;
struct termios new_t;
