/*
Author: Nicholas Leluan
CSC 452; Dr. Misurda 
Spring 2022

This file contains various test functions that demonstrate various 
functions needed to complete the assignment. This file has a main() 
class that tries to dictate to the best of its ability what test is 
being presently run and when it completes. So of these tests ask for 
user input, please follow any prompts.
*/
#include <stdio.h>
#include "graphics.h"

void test_sleep_ms();
void test_getkey();
void test_looping_chars();
void test_my_square();

int main(){
	printf("Testing test_sleep_ms()....\n");
	test_sleep_ms();
	printf("Testing test_getkey()....\n");
	test_getkey();
	printf("Testing test_looping_chars()....\n");
	test_looping_chars();
	printf("Testing test_my_square().....\n");
	printf("One moment.....\n");
	sleep_ms(3000);
	test_my_square();
	return 0;

}
/*
Quick little function that counts to 5 using the sleep_ms function and 
setting its argument to 1000 (1 second)
*/
void test_sleep_ms(){
	printf("Going to buffer for 5 seconds:\n");
	long sec = 1000;
	int x;
	for(x=1; x <= 5; x++){
		printf("%d mississippi...\n",x);
		sleep_ms(sec);
	}
}

/*
Function does no emmulate the exact functionality as it would in the 
main program, but this function simply asks the user for one input, then 
prints that input to screen. 
*/
void test_getkey(){
	printf("Entering test mode for getkey()\n");
	int ans = 0;
	do{
		char key; 
		key = getkey();
		if(key){
			printf("Pressed char: %c\n",key);
			ans = 1;
		}
	}while(ans !=1);
	sleep_ms(2000);
}

/*
Fun little function that prints three strings to the string from the top 
all the way and beyond the height of the framebuffer. 
This test tests the functionality of using the RGB function as well as 
demonstrates that drawing outside the height of the framebuffer does not 
cause a segfault 
*/
void test_looping_chars(){
	char* hello = "Hello";
	char* name = "CSC_452";
	char* goodbye = "Goodbye";
	init_graphics();
	int x;
	int y = 150;
	for(x=0;x<100;x++){
		color_t a = get_RGB_565(31,(63-x)%63,31);
		color_t b = get_RGB_565((31-x)%31,63,31);
		color_t c = get_RGB_565(31,63,(31-x)%31);
		draw_text(50,x*10,hello,a);
		draw_text(150,x*10,name,b);
		draw_text(250,x*10, goodbye, c);
	}
	printf("Displaying the text for 5 seconds before next test.\n");
	sleep_ms(5000);
	exit_graphics();

}

/*
Colorful test that draws multiple colorful squares all over the screen 
as directed by the user.
The same functionality as in square.c applies to directing the square.
*/
void test_my_square(){

	int i;
	char key;
	init_graphics();
	int x = (640-20)/2;
	int y = (480-20)/2;
	color_t back = get_RGB_565((x%31),(y%63),(x%31));
	char * message = "press 'q' to quit";
	draw_text(x, 10, message,back);
	do {
		draw_rect(x,y,20,20,back);
		key = getkey();
		if(key == 'w') y -= 10;
		else if(key == 's') y+= 10;
		else if(key == 'a') x-=10;
		else if(key == 'd') x+=10;
		back = get_RGB_565(x%31,(x-y)%61,y%31);
		draw_rect(x,y,20,20,back);
		sleep_ms(20);
	
	}while(key !='q');
	exit_graphics();

}
