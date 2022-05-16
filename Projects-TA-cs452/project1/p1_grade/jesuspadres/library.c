#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "iso_font.h"

typedef unsigned short color_t;

int fd;                           // framebuffer file descriptor
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
long int screensize = 0;
int line_length = 0;
int x = 0, y = 0;
struct termios keyEcho;           // TCGETS keystroke struct
short *map;                       // framebuffer pixel map
int ft = 0;                       // TCGETS file descriptor
tcflag_t default_flag;            // initial TCGETS flag to set back to default

/**
  Purpose: Initializes graphical operations to be performed
  in other functions. Disables ECHO and canonical mode.

  Pre-condition: None that I know of

  Post-condition: Pixels can be modified using *map, above variables
  are initialized with appropriate information, and the terminal
  no longer echos input and has canonical mode disabled.

  Parameters: None

  Returns: void
*/
void init_graphics() {
  fd = open("/dev/fb0", O_RDWR);
  if (fd == -1) {
      perror("Error: cannot open framebuffer device");
      exit(1);
  }

  // Get fixed screen information
  if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
      perror("Error reading fixed information");
      exit(2);
  }

  // Get variable screen information
  if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
      perror("Error reading variable information");
      exit(3);
  }

  line_length = finfo.line_length;
  screensize = vinfo.yres_virtual*finfo.line_length;

  map = mmap (NULL, screensize, PROT_READ | PROT_WRITE,
   MAP_SHARED, fd, 0);

  if(map == MAP_FAILED) {
    printf("Mapping Failed\n");
    exit(1);
  }

  if (ioctl(ft, TCGETS, &keyEcho) == -1) {
      perror("Error reading TCGETS");
      exit(1);
  }

  default_flag = keyEcho.c_lflag;
  keyEcho.c_lflag &= ~ECHO;
  keyEcho.c_lflag &= ~ICANON;

  if (ioctl(ft, TCSETS, &keyEcho) == -1) {
      perror("Error reading TCSETS");
      exit(1);
  }
}

/**
  Purpose: Resets ECHO and canonical mode

  Pre-condition: keyEcho and default_flagmust be initialized with the
  appropriate information.

  Post-condition: terminal input behavior is back to original settings

  Parameters: None

  Returns: void
*/
void exit_graphics() {
  keyEcho.c_lflag = default_flag;

  if (ioctl(ft, TCSETS, &keyEcho) == -1) {
      perror("Error reading TCSETS");
      exit(1);
  }
}

/**
  Purpose: Clears the screen

  Pre-condition: ft contains terminal file descriptor

  Post-condition: terminal screen is cleared of any text or color

  Parameters: None

  Returns: void
*/
void clear_screen() {
  write(ft, "\x1b[2J", 7);
}

/**
  Purpose: Gets char value of a keystroke input in the terminal

  Pre-condition: None that I know of

  Post-condition: Pixels can be modified using *map, above variables
  are initialized with appropriate information, and the terminal
  no longer echos input and has canonical mode disabled.

  Parameters: None

  Returns: void
*/
char getkey() {
  fd_set rfds;
    struct timeval tv;
    int retval;
    char c;

    FD_ZERO(&rfds);
    FD_SET(0, &rfds);

    tv.tv_sec = 10000;
    tv.tv_usec = 0;
    retval = select(1, &rfds, NULL, NULL, &tv);

    if (retval == -1) {
        perror("select()");
    } else {
        read(retval, &c, 1);
    }
    return c;
}

/**
  System calls:
  nanosleep
*/
void sleep_ms(long ms) {
   struct timespec rem;
   struct timespec req= {
       (int)(ms / 1000),
       (ms % 1000) * 1000000
   };

   nanosleep(&req , &rem);
}

void draw_pixel(int x, int y, color_t color) {
  if (x < 0 || x >= vinfo.xres_virtual || y < 0 || y >= vinfo.yres_virtual) {
    return;
  }
   unsigned short *row = map + y * finfo.line_length/2;
   row[x] = color;

}

void draw_rect(int x1, int y1, int width, int height, color_t c) {
  for (y = y1; y < y1+height; y = y++)
  {
    for (x = x1; x < x1+width; x++)
      draw_pixel(x, y, c);
  }
}

void draw_letter(int x, int y, const char letter, color_t c) {
  int i;
  int j;
  for (j = 0; j < 16; j++) {
    int lPixel = iso_font[letter*16 + j];
    for (i = 7; i >= 0; i--) {
      int n = (lPixel>>i) & 1;
      if (n) {
        draw_pixel(x+i, y+j, c);
      }
    }
  }
}

void draw_text(int x, int y, const char *text, color_t c) {
  int i;
  for (i = 0; text[i]; i++) {
    draw_letter(8*i+x, y, text[i], c);
  }
}
