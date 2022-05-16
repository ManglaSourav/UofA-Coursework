//
// Created by Sourav Mangla on 02/02/22.
//
#include<stdio.h>
#include "iso_font.h"
#include "iostream"

using namespace std;

void draw_text(){
    char * text ="Testing writing text";
    int x=10;
    int y=10;
    int n=0;
    int c =63488;
    char t = text[n];
    while(t!='\0'){ /* iterate over the whole string */
        int i;
        for(i=0; i<16; i++){ //going to 16 row of a character
            unsigned char byte = iso_font[ 16 * text[n] + i]; // 16*ascii+i

            for(j = 0; j < 8; j++){ //Iterate left through right in this specific row
                int draw_pixel_boolean = curr_row & 0x01; //AND the current row with 0x80 (1000 0000) to get the most significant bit
                curr_row >>= 1; //Shift the current row value right by 1 so we can continue to grab the least significant bit

                if(draw_pixel_boolean){ //If our AND'd bit is 1, draw the pixel there
//                    draw_pixel(x + j, y + i, c);
                }
            }

//            cout<<16 * text[n] + i<<endl;
//            int j;
//            int k=0;
//            for(j=1; j<=128; j*=2){
//                if(byte & j)//bitwise and
//                    printf("%d %d ",x+i, y+k);
//                    printf("\n");
//                    k++;
//            }
        }
        break;

        t = text[++n];
        y+=8;
    }
}


void pritnBinary(int n){
    while (n) {
        if (n & 1)
            printf("1");
        else
            printf("0");

        n >>= 1;
    }
    printf("\n");
    return;
}


//Draw a given character found in the iso_font array with given color c at location (x, y)
void draw_char(int x, int y, const char character, color_t c){
    int i = 0;
    int j = 0;

    for(i = 0; i < 16; i++){ //Iterate through the rows
        int curr_row = iso_font[character*16 + i]; //Get the pixel data for row i

        for(j = 0; j < 8; j++){ //Iterate left through right in this specific row
            int draw_pixel_boolean = curr_row & 0x01; //AND the current row with 0x80 (1000 0000) to get the most significant bit
            curr_row >>= 1; //Shift the current row value right by 1 so we can continue to grab the least significant bit
            if(draw_pixel_boolean){ //If our AND'd bit is 1, draw the pixel there
                draw_pixel(x + j, y + i, c);
            }
        }
    }
}

//Draw a given piece of text onto the display at location (x, y) and color c
void draw_text(int x, int y, const char* text, color_t c){
    int i = 0;

    for(i = 0; text[i] != '\0'; i++){ //Iterate through the charactesr in the text
        draw_char(x + i*8, y, text[i], c); //Draw each character, moving 16 pixels to the right each time
    }
}
int main(){
    draw_text();
    return 0;
}








=