/*
*File:    driver.c
*Author:  Austin Connick
*Class:   csc452
*Purpose: This file is to test the library.c
*         by having the user try and guess a letter picked by the program
* 
*/

#include "graphics.h"


int main(int argc, char** argv){


    init_graphics();
    const char a[] = "This is a test of the graphics";
    const char b[] = "Can you guess my letter";
    const char c[] = "press 1 to quit";
   // const char d[] = "press r to hide this";
    const char e[] = "sorry that is not my letter try again";
    const char f[] = "Awesome that is my letter";
    const char g[] = "let me think of another one";
    const char h[] = "my letter is before that";
    const char j[] = "my letter is after that";
    const char k[] = "press a letter to start";
     
    const char *aa = a;
    const char *bb = b;
    const char *cc = c;
   // const char *dd = d;
    const char *ee = e;
    const char *ff = f;
    const char *gg = g;
    const char *hh = h;
    const char *jj = j;
    const char *kk = k;
    
    draw_text(10,10,aa,6000);
    draw_text(10, 30,bb,63400);
    draw_text(10,50,cc,63488);
    draw_text(10,110,kk,6000);
    char key;
    draw_rect(4,6,500,76, 6000);
    char let = 'a' + (random() % 26);


  
    while(key != '1'){
        
        key = getkey();
        if(key == let){
            draw_text(10,110,kk,0);
            draw_text(10,90,ee,0);
            draw_text(10,110,jj,0);
            draw_text(10,110,hh,0);
        
        
        

            draw_text(10,90,ff,63400);
            draw_text(10,110,gg,63400);
            let = 'a' + (random() % 26);
       
        
        
        }else if(key >= 'a' && key <= 'z'){
            draw_text(10,110,kk,0);
            draw_text(10,90,ff,0);
            draw_text(10,110,gg,0);
            draw_text(10,90,ee,63488);
            if(let > key){
                draw_text(10,110,hh,0);
                draw_text(10,110,jj,63400);
            }else{ 
                draw_text(10,110,jj,0);       
                draw_text(10,110,hh,63400); 
            }    
        }
           

        sleep_ms(20);        
    

    }
    
    exit_graphics();
return 0;

}
