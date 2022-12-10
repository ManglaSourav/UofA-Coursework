/*
*File:    library.c
*Author:  Austin Connick
*Class:   csc452
*Purpose: This file is a basic graphics library for set pixel to colors
*         drawing shapes and reading keypresses, using system calls
*/
#include<sys/mman.h>
#include<sys/time.h>
#include<termios.h>
#include<sys/ioctl.h>
#include<fcntl.h>
#include<unistd.h>
#include<linux/fb.h>
#include "iso_font.h"

int screen_x;
int screen_y;
int map_size;
int line_size;
typedef unsigned short color_t;
void *addr;
int fp;
int length;
struct termios prset;
/*
* init_graphics opens the framebuffer and maps it to memory
* turns of echo and cleans the screen
*/
void init_graphics(){

    //open graphics
    //int fp;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    struct termios ter;
    //int length = 12;
    fp = open("/dev/fb0", O_RDWR);
 
    //map mem
    //char *addr;
    //addr = mmap(NULL, length, PROT_READ | PROT_WRITE ,MAP_SHARED,fp,0);

    //find screen stats
    ioctl(fp,FBIOGET_VSCREENINFO, &vinfo);
    ioctl(fp,FBIOGET_FSCREENINFO, &finfo);
    screen_x = vinfo.xres_virtual;
    screen_y = vinfo.yres_virtual;
    line_size = finfo.line_length;
    map_size = screen_y * finfo.line_length;
    length = map_size;
    //map men
    addr = mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);


    //disable keypress
    ioctl(0,TCGETS,&prset);
    ter = prset;
    ter.c_lflag &= ~(ICANON|ECHO);

    ioctl(0,TCSETS,&ter);

    write(1,"\033[2J",7);
}
/*
* exit_graphics reset echo and munmap and frees the framebuffer
*/
void exit_graphics(){



    write(1,"\033[2J",7);

    ioctl(0,TCSETS,&prset);

    munmap(addr,length);
    close(fp);
}
/*
* clear_screen clear the screen by writing escape code to stdout
*/
void clear_screen(){
    write(1,"\033[2J",7);

}
/*
* getkey finds if a key has been pressed and then returns the 
* the char
*/
char getkey(){
    struct timeval tim;
    fd_set rf;
    FD_ZERO(&rf);
    FD_SET(0,&rf);
    tim.tv_sec = 0;
    tim.tv_usec = 0;
    if(select(1,&rf, 0, 0,&tim)){
//read
        int end;
        char buf[1];
        end = read(0, buf,1);
        return buf[0];
    }else{
    //no input
        return;
    }

}
/*
* sleep_ms has the system sleep for a number of milliseconds
*/
void sleep_ms(long ms){
    struct timespec req;
    req.tv_sec = 0;
    req.tv_nsec = ms * 1000000;
    nanosleep( &req, 0);

}
/*
* draw_pixel find were a pixel should be give an x y position
* then sets that loction in the buffer to a given color
*/
void draw_pixel(int x,int y, color_t color){
    if(x > 0 && x < screen_x && y > 0 && y < screen_y){ 
        int pixel = (x * (16/8)) + y * line_size;
        *((int*)(addr + pixel)) = color;
    }
}
/*
* draw_rect draws a rectangle starting at a x y position with a 
* width and height
*/
void draw_rect(int x1, int y1, int width, int height, color_t c){
//top
    int start;
    int i = x1;

    for(i = x1; i <= (x1 + width); i++){ 
        draw_pixel(i,y1,c);
    }
//left
    int k =y1;
    for(k = y1; k <= (y1 + height); k++){
        draw_pixel(x1,k,c);


    }
//right
    int j = y1;
    for(j = y1;j <= (y1 + height); j++){
        draw_pixel((x1+width),j,c);

    }
//bottom    
    int l = x1;
    for(l =x1; l <= (x1+width); l++){
        draw_pixel(l,(y1+height),c);
    }
}
/*
* draw_char is a helper for draw_text that takes a x and y and 
* draws a char from iso_font, at that location 
*/
void draw_char(int x, int y, char ch, color_t c){
    int in_start = (ch * 16 + 0);
    int in_end = (ch * 16 + 15);
    int i , k, line;
    for(i = y; i < y + 16; i++){
        line = iso_font[in_start];
            for(k = x; k < x + 8; k++){

                if(line & 0x01){
                    draw_pixel(k,i,c);


                }
            line = line >> 1;
            }
       in_start++;
    }

}
/*
* draw_text pass over a string and uses draw_char to draw each char
* at a starting x y position anding a small space between chars
*/
void draw_text(int x, int y, const char *text, color_t c){
    const char * a;
    for( a = text; *a != '\0'; a++){


        draw_char(x,y, *a,c);
        //space
        x = x + 10;
    }
}















