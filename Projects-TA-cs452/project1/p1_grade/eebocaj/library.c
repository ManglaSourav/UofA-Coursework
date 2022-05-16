/*
 * File library.c
 * Author: Jacob Edwards
 * This library contains functions for displaying very basic graphics
 * on the screen.
 *
 * We have implemented this using 2 screenbuffers.  We write to the back
 * buffer, and then copy the changes to the buffer representing the full
 * screen.  This prevents us from having a strobe effect on our display.
 */
#include "iso_font.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

typedef unsigned short color_t;

struct FrameBuffer {
	int file;
	int fb_width;
	int fb_height;
	int size;
	int needsUpdate;
	color_t *active;
	color_t *back;
} frameBuffer;

void init_graphics();
void exit_graphics();
void clear_screen();
char getkey();
void sleep_ms(long ms);
void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
void draw_text(int x, int y, const char *text, color_t c);
void draw_letter(int x, int y, const char letter, color_t c);
void update_frame();

void printBin(unsigned n);
void printBinHelper(unsigned n);
color_t generateColorCode(int r, int g, int b);

void init_graphics() {
	/*
	 * Initializes the graphics engine.  Loads the framebuffer
	 * and disable keypress echo.
	 */

	// Finding the framebuffer for the screen
	int fp = open("/dev/fb0", O_RDWR);

	// Loading the Frame Buffer into Memory
	struct fb_var_screeninfo vinfo;
	ioctl(fp, FBIOGET_VSCREENINFO, &vinfo);
	int fb_width = vinfo.xres;
	int fb_height = vinfo.yres;
	int fb_bytes = vinfo.bits_per_pixel / 8;

	int buffer_size = fb_width * fb_height * fb_bytes;
	color_t *fbdata = mmap(0, buffer_size, PROT_READ | PROT_WRITE,
	MAP_SHARED, fp, 0);
	color_t *fbback = mmap(0, buffer_size, PROT_READ | PROT_WRITE,
	MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	// Disabling keypress echo
	struct termios term;
	tcgetattr(STDIN_FILENO, &term);
	term.c_lflag &= ~(ECHO | ICANON);
	if (tcsetattr(STDIN_FILENO, TCSANOW, &term) != 0) {
		printf("%s\n", strerror(errno));
	}

	// Saving the framebuffer for later use
	frameBuffer.file = fp;
	frameBuffer.active = fbdata;
	frameBuffer.back = fbback;
	frameBuffer.fb_width = fb_width;
	frameBuffer.fb_height = fb_height;
	frameBuffer.size = buffer_size;
};

void exit_graphics() {
	/*
	 * Performs shutdown tasks for closing the graphics engine.
	 * Unmaps the framebuffer from memory and enables keypress
	 * echo.
	 */

	// freeing framebuffer memory
	munmap(frameBuffer.active, frameBuffer.size);
	munmap(frameBuffer.back, frameBuffer.size);

	// closing the framebuffer file
	close(frameBuffer.file);

	// enable ECHO and ICANON
	struct termios term;
	tcgetattr(STDIN_FILENO, &term);
	term.c_lflag |= (ECHO | ICANON);
	if (tcsetattr(STDIN_FILENO, TCSANOW, &term) != 0) {
		printf("%s\n", strerror(errno));
	}
};

void clear_screen() {
	/*
	 * Sets every pixel in the screen to black.
	 * More accurately, sets every bit in the framebuffer to 0.
	 */

	memset(frameBuffer.back, 0, frameBuffer.size);
};

char getkey() {
	/*
	 * Finds a keypress event and returns the associated character.
	 */

	fd_set rfds;  // read file descriptors
	struct timeval tv;
	int retval;

	// Watch stdin to see when it has input
	FD_ZERO(&rfds);  // clear the set
	FD_SET(0, &rfds);  // add stdin to set

	// setting the wait
	tv.tv_sec = 0;
	tv.tv_usec = 1000000;  // nanoseconds

	retval = select(1, &rfds, NULL, NULL, &tv);
	//  tv may have changed after this

	if (retval == -1) {
		printf("%s\n", strerror(errno));
	} else if (retval) {
		char key = getchar();
		// flushing the input buffer
		return key;
	}

	return '\0';
};

void sleep_ms(long ms) {
	/*
	 * Sleeps the active thread for the specified time.
	 */

	struct timespec ts;
	ts.tv_sec = ms / 1000;
	ts.tv_nsec = (ms % 1000) * 1000000;

	nanosleep(&ts, NULL);
};

void draw_pixel(int x, int y, color_t color) {
	/*
	 * Updates the color of the specified pixel on the screen
	 */
	if (x < 0 || x >= frameBuffer.fb_width ||
			y < 0 || y >= frameBuffer.fb_height) {
		return;
	}
	int offset = (y * frameBuffer.fb_width + x);
	color_t* pos = frameBuffer.back + offset;
	*pos = color;
	frameBuffer.needsUpdate = 1;
};

void draw_rectangle(int x0, int y0, int width, int height,
			color_t color) {
	/*
	 * Draws a rectangle to the screen at the specified location
	   of the specified size and color.
	 */
	int x, y;
	for (x = x0; x < x0 + width; x++) {
		for (y = y0; y < y0 + height; y++) {
			draw_pixel(x, y, color);
		}
	}
}

void draw_text(int x, int y, const char *text, color_t color) {
	/*
	 * Displays the specified text on the screen at the specified
	 * location.
	 */
	int off = 0;
	while (*text) {
		draw_letter(x + off, y, *text, color);
		off += 8;
		text++;
	}
};

void update_frame(int fps) {
	/*
	 * Updates the active framebuffer with new graphics context.
	 * This function MUST be called in order for any changes to
	 * appear on the screen.
	 *
	 * Use fps param to set desired animation speed.
	 */
	if (!frameBuffer.needsUpdate) return;
	memcpy(frameBuffer.active, frameBuffer.back, frameBuffer.size);

	sleep_ms(1 / fps * 1000);
}

/* ------- Helper Functions ----------------------------------------- */

// prints a binary representation of an integer
void printBin(unsigned n) {
	printBinHelper(n);
	printf("\n");
};

// recursive printing function
void printBinHelper(unsigned n) {
	if (n > 1)
		printBinHelper(n / 2);
	printf("%d", n % 2);
}

// generates a 16 bit color integer from r, g, b vals 0-255
color_t generateColorString(int r, int g, int b) {
	color_t color16 = ((r / 8) << 11) + ((g / 4) << 5) + (b / 8);
	return color16;
};

// draw a letter on the screen given position and color
void draw_letter(int x, int y, const char letter, color_t color) {
	char *letterArr = (char *) iso_font + letter * 16;
	int yoff;
	for (yoff = 0; yoff < 16; yoff++) {
		char row = *(letterArr + yoff);

		int xoff;
		for (xoff = 0; xoff < 8; xoff++){
			int isBitLit = (row >> xoff) % 2;
			if (isBitLit)
				draw_pixel(x + xoff, y + yoff, color);
		}
		printf("\n");
	}
};
