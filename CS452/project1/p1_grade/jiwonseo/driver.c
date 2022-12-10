/**
Name: Jiwon Seo
Project: CSC 452 project 1
Spring 2022, by February 6th,2022, 11:59pm
Gmae is called "Catch the camellon monster", because when you catch the moster "M",
It will change the color like a camellon.

How to play the game: The direction key are same as square.c "a" for left,
"w" for up, "d" for right, "s" for down.

By the default color blue, the M is changing its color to blue,
If you chage the color of box by "R", which is Red, the monster color would chnage to Red as well.
If you change the color of box by "G", which is Green, the monster color would change to Green as well.
If you change the color of box by "B", which is blue, the mosnter color would change to blue as well.

*/
typedef unsigned short color_t;

void clear_screen();
void exit_graphics();
void init_graphics();
char getkey();
void sleep_ms(long ms);

void draw_pixel(int x, int y, color_t color);
void draw_rect(int x1, int y1, int width, int height, color_t c);
int main(int argc, char** argv)
{
	int i;
  //by clearing screen, the QEMU screen is set for the game play.
  clear_screen();
  init_graphics();
	char key;
	int x = (640-20)/2;
	int y = (480-20)/2; //rectangle are set in the middle of the screen.
  draw_text(30,50,"M",0x7E0); //monster location.
  draw_text(60,80,"M",0x7E0);
  draw_text(100,120,"M",0x7E0);
  draw_text(120,240,"M",0x7E0);
  color_t c = 0xFF; //color set for blue.
  do
  {
    draw_rect(x,y,20,20,0); //this rectangle is to erase the previous drawing.
    key = getkey(); //user input key
    if(key == 'w') y-=10;
    else if(key == 's') y+=10;
    else if(key == 'a') x-=10;
    else if(key == 'd') x+=10;
    else if(key == 'R') c= 0xF800; //change color to red.
    else if(key == 'G') c=0x7E0; //change color to green
    else if(key == 'B') c=0x1F; //change color to Blue
    if(x==30 && y==50){
      draw_text(30,50,"M",c); //it will change the text color by the color c if user hit that location.

    }else if(x==60 && y==80){
      draw_text(60,80,"M",c);

    }else if(x==100&&y==120){
      draw_text(100,120,"M",c);

    }else if(x==120 && y==240){
      draw_text(120,240,"M",c);

    }

    draw_rect(x,y,20,20,c); //draw the rectangle by user movement.
    sleep_ms(20);


  } while(key != 'q');

  exit_graphics();

  return 0;

  }
