
/**
 * Author: Zachary Florez 
 * Course: CSC 452 
 * File: library.c
 * Description: This file contains the implementation of all of the functions 
 *              needed for the project 1, using pure linux system calls and no 
 *              C Standard Library Calls. 
 * 
 */

#include "iso_font.h" 
#include "graphics.h"
#include <linux/fb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>


struct fb_var_screeninfo resolution; 
struct fb_fix_screeninfo bit_depth;  
struct termios termios; 
int fb;
int size; 
color_t * frame_buffer; 
int line_x;
int line_y; 


/**
 * This function does all the work necessary to initialize the grpahics library. 
 * Including 4 things. 
 * 
 * 1. Opening the graphics device. 
 * 2. Using the idea of memory mapping for our library with the mmap() system call. 
 * 3. Knowing there are 640 bits in the x direction and 480 bits in the y direction. 
 *    Where we can use the system call ioctl to get the screen size amd bits per pixel. 
 * 4. Use the ioctl system call to disable keypress echo and buffering the keypress. 
 *    Here the commands we use are TCGETS and TCSETS. 
 * 
 */
void init_graphics() {
    
    // Open the graphics device, the first framebuffer 
    fb = open("/dev/fb0", O_RDWR);

    // If the framebuffer isn't there exit. Otherwise proceed with 
    // our input output control.
    if (fb == -1) { return; } 

    if (ioctl(fb, FBIOGET_VSCREENINFO, &resolution) != -1) {
	    if (ioctl(fb, FBIOGET_FSCREENINFO, &bit_depth) != -1) {

		    // Set these vars so we can use it for other functions 
		    line_x = bit_depth.line_length; 
		    line_y = resolution.yres_virtual; 

		    // total size of the mmaped file  
		    size = bit_depth.line_length * resolution.yres_virtual; 
	    }
    } else { 
	    return; 
    }

    // Create frame buffer using mmap with reading and writing to to pages. 
    // Also Using MAP_SHARED since the updates to mapping are visible to other processes. 
    frame_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
	
    // Clear the Screen 
    // file code, buffer num bytes
    write(STDOUT_FILENO, "\033[2J", 4);


    // Final step is to use ioctl system call to disable keypress echo and buffering 
    // the keypress. 
    // USE: TCGETS and TCSETS 
    // 			termios ==> struct that describes current terminal settings
    ioctl(STDIN_FILENO, TCGETS, &termios); 
    termios.c_lflag &= ~(ECHO | ICANON); 
    ioctl(STDIN_FILENO, TCSETS, &termios); 

}


/** 
 * This function is what we are going to use for when we want to undo 
 * whatever it is that needs to be cleaned up before the program exits. 
 * 
 * We will use close() for files, use munmap() to unmap memory, and use ioctl() 
 * to reenable key press echoing and buffering as needed. 
 *  
 */ 
void exit_graphics() {

    // Unmap the memory 
    // munmap(void* address, size_t length) 
    munmap(frame_buffer, size); 

    // ioctl to reenable key press echoing and buffering 
    if (ioctl(STDIN_FILENO, TCGETS, &termios) == -1) {
	    return; 
    } 

    termios.c_lflag |= ECHO; 
    termios.c_lflag != ICANON; 

    if (ioctl(STDIN_FILENO, TCSETS, &termios) == -1) {
	    return; 
    }

    // Close the file 
    close(fb);

}


/**
 * Here we are going to use ANSI escape code to clear the screen rather than
 * blanking it by drawing a giant rectangle of black pixels. 
 * 
 * See spec for description on how ANSI will work. 
 * 
 */
void clear_screen() {

    // write(int fd, const void *buf, size_t count) 
    write(1, "\033[2J", 4); 
}


/**
 * For this function, in order for us to make games, we need to have user input 
 * And we will use key press input to read single characters using the read() 
 * system call. 
 * 
 * Be aware that read() blocks and this will not allow our program to draw unless
 * the user has typed something. So if there is a key press at all we will read it
 * using the linux non-blocking system call select(). 
 * 
 */
char getkey() {
    
    fd_set fds;
    struct timeval tv; 
    int retval = 0; 

    FD_ZERO(&fds); 
    FD_SET(STDIN_FILENO, &fds); 

    tv.tv_sec = 0; 
    tv.tv_usec = 0; 

    char key;

    // select(int nfds, fd_set *readfds, fd_set *writefds, fd_set, *exceptfds, struct timeval *timeout) 
    retval = select(1, &fds, NULL, NULL, &tv); 

    if (retval > 0) {

        // read(int fd, void* buff, size_t count)
	    read(0, &key, sizeof(key)); 
	    return key; 

    } else {
	    return 0; 
    }

}


/**
 * For this function we will use the system call nanosleep() to make our program 
 * sleep between frames of graphics being drawn. We will sleep for a specified 
 * number of miliseconds and just multiply that by 1,000,000. 
 * 
 * Set the second parameter for nanosecond to NULL since we don't care about the 
 * call being interupted. 
 * 
 */
void sleep_ms(long ms) {

    struct timespec req; 
    req.tv_nsec = ms * 1000000; /* nanoseconds */ 
    req.tv_sec  = 0;            /*   seconds   */ 
    
    // Man page how to use nanosleep
    // int nanosleep(const struct timespec *req, struct timespec *rem); 
    nanosleep(&req, NULL); 

}


/**
 * This function is the main drawing code, where the work will actually be done.
 * We want to set the pixel at coordinate (x, y) to the specified color. 
 * 
 * Then use those coordinates to scale the base address of the memory mapped 
 * framebuffer using pointer arithmetic. The framebuffer is stored in row-major 
 * order, so the first row starts at offset 0, followed by the second row of pixels
 * and so on. 
 * 
 */
void draw_pixel(int x, int y, color_t color) {
    
	// First we check our x and our y coords to make sure they're still in bounds
	if (x <= 0 || x >= line_x || y <= 0 || y >= line_y) {
		return; 
	}

	// Otherwise we can proceed with our calculations 
	color_t* fb = (color_t*) frame_buffer + ( (y * line_x) + x); 
	*fb = color; 
}


/**
 * For this function, we will use draw pixel to make a rectangle with corners:
 *
 * ( x1  ,     y1    )              ( x1+width   ,   y1    )
 * 
 * ( x1  , y1+height )              ( x1+width   ,  y1+height)
 * 
 */
void draw_rect(int x1, int y1, int width, int height, color_t color) {
	
	int x = x1; 
	int y = y1; 
	int i;
	int j; 
	
	// Iterate through the draw the triangle. 
	for (i = y1; i < (y1 + height) - 1; i ++) {
		for (j = x1; j < (x1 + width) - 1; j ++) {
			draw_pixel(j, i, color); 
			j += 1; 
		} 
		i += 1; 
	}
}


/**
 * This function is a helper function called from draw_text used to draw 
 * just a single character one at a time. 
 * 
 * We cannot use strlen() since it is a C standard library function, instead 
 * this implementation just iterates until we find '\0' inside of our 
 * array, meaning we're at the end of the array. 
 * 
 */
void draw_single_character(const char* word, int x, int y) {
	
    	int letter = word[0]; 	
	int index = letter * 16; 
	int last_index = (letter * 16) + 16; 

	// Now loop through the 15 rows to shift
	while (index < last_index) {
		int curr = iso_font[index];
		index += 1; 
	}	
}


/**
 * This function is used to draw the string with the specified color at the 
 * starting location (x, y). 
 *
 * We use a font encoded into an array iso_font.h. Every letter is encoded as
 * 16 1-byte integers. 
 * 
 */
void draw_text(int x, int y , const char* text, color_t color) {
    
	const char *word;
        word = text;
        while (word != '\0') {  
		draw_single_character(word, x, y); 
		word ++; 
		x += 20;
	}
}






