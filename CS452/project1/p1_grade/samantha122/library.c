/* File: library.c
 * Author: Samantha Mathis
 * Purpose: creates a graphics library, using linx system calls. 
 */


#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include "iso_font.h"
#include "graphics.h"

int fp;
int console;
color_t *ptr;
struct termios term;

/**
 * This method clears the screen
 **/
void clear_screen(){
    write(1, "\033[2J", 8);
}

/**
 * This method draws an individula pixel
 * x: is the x coordnate of the pixel
 * y: is the y coordinate of the pixel
 * color: is the color of the pixel
 **/ 
void draw_pixel(int x, int y, color_t color){
    if (y < 480 && y > 0){
        ptr[y*640+x] = color;
    }   
    
}

/**
 * This method draws and fills in a rectangle
 * x: is the x coordnate of the rectangle
 * y: is the y coordinate of the rectangle
 * width: is the width of the rectangle
 * height: is the height of the rectangle
 * color: is the color of the rectangle
 **/ 
void fill_rect(int x, int y, int width, int height, color_t color){
    int i;
    for(i = x; i <= x+width; i++){
        int j;
        for (j = y; j <= y+height;j++){
            draw_pixel(i, j, color);
        }
        
   }
}
/**
 * This method draws a rectangle
 * x: is the x coordnate of the rectangle
 * y: is the y coordinate of the rectangle
 * width: is the width of the rectangle
 * height: is the height of the rectangle
 * color: is the color of the rectangle
 **/ 
void draw_rect(int x, int y, int width, int height, color_t color){
    int i;
    for(i = x; i <= x+width; i++){
        int j;
        for (j = y; j <= y+height;j++){
            if (i == x|| i == x+width){
                draw_pixel(i, j, color);
            }else if (j == y || j == y+height){
                draw_pixel(i, j, color);
            }
        }
        
   }
}

/**
 * This method writes text using iso_font
 * x: is the x coordnate of the first letter
 * y: is the y coordinate of the first letter
 * text: is what you want to write
 * color: is the color of the text
 **/ 
void draw_text(int x, int y, const char *text, color_t c){
    int original_x = x;
    int original_y = y;
    int i = 0;
    //Iterates through each letter of the text
    while (text[i] != '\0'){
        int ascii = text[i];
        int j;
        //Loops through the rows from the iso_font array
        for (j = 0; j < 16; j++){
            //Finds the appropriate spot to index based on the ascii letter
            char result = iso_font[ascii * 16 + j];
            int k;
            x = original_x;
            //Finds which pixel to draw, using bit masking and shifting
            for (k = 8; k >=0 ;k--){
                char shiftleft = result << k;
                char shiftright = shiftleft >> 7;
                if (shiftright == 0){
                    //Black pixel
                    draw_pixel(x, y, 0);
                }else if (shiftright == 1){
                    //Colored pixel
                    draw_pixel(x, y, c);
                }
                x++;
            }
            y++;
            
        }
        original_x += 8;
        y = original_y;
        i++;
    }

}

/**
 * This method intializes the graphics
 * It opens and maps the framebuffer, clears the screen
 * retrieves the console and disables echo and icanon
 **/ 
void init_graphics(){
    // Open the framebuffer file
    char* filename = "/dev/fb0";
    fp = open(filename, O_RDWR);
    if (fp < 0) {
        //FILE FAILED TO OPEN
    }
    // Map frambuffer into array.
    ptr = mmap(NULL, 640*480*sizeof(color_t), PROT_READ | PROT_WRITE, MAP_SHARED, fp, 0);
    if (ptr == NULL) {
      //ERROR MAPPING MEMORY
    }

    clear_screen();
    
    char* console_name = "/dev/tty";
    console = open(console_name, O_RDWR);
    if (console < 0) {
      //Getting the console failed
    }

    int result = ioctl(console, TCGETS, &term);
    if (result < 0) {
       //TCGETS failed
    }

    //disables key press echo and icanon
    term.c_lflag &= ~(ICANON|ECHO);
    result = ioctl(console, TCSETS, &term);
    if (result < 0) {
        //TCSETS failed
    }

}

/**
 * This method grabs the key that is pressed by using select
 * first in order not to block the drawing of the graphics 
 * once something is there to select it will then read the character
 * returns a char which is the key that was pressed
 **/ 
char getkey(){
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    //Sets to 0 so there is no delay
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int result = select(1, &rfds, 0, 0, &tv);
    if (result == -1){
       //error  
    }else if (result){
       char key;
       ssize_t readin = read(0, &key, 1);
       if (readin != -1){
           return key;
       }
    } else {
        //nothing
    }
    return "";
}

/**
 * This method sleeps for a specified number of miliseconds
 * time: is a long integer of how many miliseconds to sleep for
 **/ 
void sleep_ms(long time){
    struct timespec ts;
    ts.tv_sec = 0;
    //converts nanosecs to miliseconds
    ts.tv_nsec = time *1000000;
    nanosleep(&ts, NULL);
}

/**
 * This methods exits the graphics and retores echo and icanon
 * unmaps the frame bit and closes the console
 **/ 
void exit_graphics(){
    term.c_lflag |= (ECHO|ICANON);
    int result = ioctl(console, TCSETS, &term);
    if (result < 0) {
        //Failed to set ICANON on exit
    }

    
    int err = munmap(ptr, 640*480*sizeof(color_t));
    if (err != 0) {
        //UNMAPPING FAILED
    }
    close(fp);
    close(console);
}

