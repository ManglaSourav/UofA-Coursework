//
// Created by Carter Boyd on 2/6/22.
//

#ifndef CSC452_GRAPHICS_H
#define CSC452_GRAPHICS_H

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/termios.h>
#include "iso_font.h"

#define ESCAPE_CODE "\033[2J"
#define FRAME_BUFFER "/dev/fb0"
#define STDIN 0
#define STDOUT 1
typedef unsigned short color_t;

void clear_screen();

void exit_graphics();

void init_graphics();

char getkey();

void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);

void draw_rect(int x1, int y1, int width, int height, color_t c);

void draw_text(int x, int y, char *text, color_t c);

#endif //CSC452_GRAPHICS_H
