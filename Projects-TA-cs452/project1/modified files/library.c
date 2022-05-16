

#include<time.h>
#include<linux/fb.h>
#include<sys/ioctl.h>
#include<termios.h>
#include<fcntl.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<stdint.h>
#include"iso_font.h"
#inclue<stdio.h>

typedef uint16_t color_t;

void * graphics_map;
int graphics_fd;
struct fb_fix_screeninfo fixed;
struct fb_var_screeninfo var;
struct termios term;

void init_graphics() {

    /* get the file discripter for the framebuffer*/
    graphics_fd = open("/dev/fb0", O_RDWR);

    /*find out what kind of screen we're using*/
    ioctl(graphics_fd, FBIOGET_VSCREENINFO, &var); //virtual resolution
    ioctl(graphics_fd, FBIOGET_FSCREENINFO, &fixed); // bit depth

    /*find out how big that screen really is*/
    int mmsize = fixed.line_length * var.yres;

    /*memory mapping of frame buffer*/
    graphics_map = mmap(NULL, mmsize, PROT_READ | PROT_WRITE, MAP_SHARED, graphics_fd, 0);

    /*now to deal with that pesky terminal*/
    ioctl(0, TCGETS, &term);
    struct termios new_term;
    new_term = term;
    new_term.c_lflag &= ~ECHO;
    new_term.c_lflag &= ~ICANON;
    ioctl(0, TCSETS, &new_term);
}

void exit_graphics(){
    /*Let's leave things how we found them*/
    munmap(graphics_map, 0); //unmap the mmap
    close(graphics_fd);      //close the fd

    /* and fix that terminal */
    ioctl(0,TCSETS, &term);
}

void clear_screen(){
    write(1, "\033[2J",7);
}

char getkey(){
    /* get key will return null if nothing is ready to read */
//    If both fields of the timeval structure are zero, then select() returns immediately.
    struct timeval time = { 0L, 0L}; //second and microsecond
    fd_set fds;
    /* Watch stdin (fd 0) to see when it has input. */
    FD_ZERO(&fds); // clears a set.
    FD_SET(0, &fds); //add a given file descriptor from a set
    int val=0;

    val=select(1, &fds, NULL, NULL, &time); //returne descriptor sets

    if(val>0){ /* we can read */
        char input;
        read(0, &input, sizeof(input)); //size of input is 1 byte.
        return input;
    }
    else{ /* not ready yet */
        return NULL;
    }
}

void sleep_ms(long ms){
    struct timespec time; //second and nanoseconds
    time.tv_sec=ms/1000;
    time.tv_nsec=(ms%1000)*1000000;
    nanosleep(&time, NULL);
}

void draw_pixel(int x, int y, color_t color){
    printf("%d", fixed.line_length);
    int loc = x * fixed.line_length + y*2;
    color_t* pixel = (graphics_map + loc);
    *pixel = color;
}

void draw_rect(int x, int y, int width, int height, color_t c){
    int i;
    for(i=x; i<x+width; i++){
        draw_pixel(y, i, c);
        draw_pixel(y+height, i, c);
    }

    for(i=y; i<=y+height; i++){
        draw_pixel(i, x, c);
        draw_pixel(i, x+width, c);
    }
}

void draw_text(int x, int y, const char* text, color_t c){

    int n=0;
    char t = text[n];
    while(t!='\0'){ /* iterate over the whole string */

    int i;
    for(i=0; i<16; i++){
        unsigned char byte = iso_font[ 16 * text[n] + i];
        int j;
        int k=0;
        for(j=1; j<=128; j*=2){
            if(byte&j)
                draw_pixel(x+i, y+k, c);
            k++;
        }
    }
    t = text[++n];
    y+=8;
    }
}

void fill_circle(int x0, int y0, int r, color_t c){

    while(r>0){  /* keep making smaller circles, until its full */
    int x = r;
    int y = 0;
    int decision_over_2 = 1-x;
    
    while(y<=x){
        draw_pixel(  x+x0,  y+y0, c);
        draw_pixel(  y+x0,  x+y0, c);
        draw_pixel( -x+x0,  y+y0, c);
        draw_pixel( -y+x0,  x+y0, c);
        draw_pixel( -x+x0, -y+y0, c);
        draw_pixel( -y+x0, -x+y0, c);
        draw_pixel(  x+x0, -y+y0, c);
        draw_pixel(  y+x0, -x+y0, c);
        y++;
        if(decision_over_2<=0){
            decision_over_2 += 2 * y + 1;
        }
        else{
            x--;
            decision_over_2 += 2 * (y-x) + 1;
        }
    }
    r--;
    }
    if(r==0){ /* if there is no radius it's just a point */
        draw_pixel(x0,y0,c);
    }
}
