#ifndef H_LIBRARY
#define H_LIBRARY
/*
 * Author: Winston Zeng
 * File: library.h
 * Class: CSC 452, Spring 2022
 * Project 1: Graphics Library
 */

    typedef unsigned short color_t;
    void clear_screen();
    void exit_graphics();
    void init_graphics();
    char getkey();
    void sleep_ms(long ms);

    void draw_pixel(int x, int y, color_t color);
    void draw_letter(int x, int y, const char *letter, color_t c);
    void draw_text(int x, int y, const char *text, color_t c);
    void draw_rect(int x1, int y1, int width, int height, color_t c);
    void fill_rect(int x1, int y1, int width, int height, color_t c);
#endif
