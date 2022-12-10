/*
Orlando Rodriguez
CSC 452 
Driver Program for assg.1
*/

// Include my header file
#include "library.h"

// Function I never ended up using
void changeColour(color_t*, color_t, color_t, color_t, color_t, color_t, color_t);

/*
Main function for the driver program
*/
int main(int argc, char** argv) {
	// Define colour constants
	const color_t red = encodeRGB(31, 0, 0);
	const color_t green = encodeRGB(0, 63, 0);
	const color_t blue = encodeRGB(0, 0, 31);
	const color_t white = encodeRGB(31, 63, 31);
	const color_t black = encodeRGB(0, 0, 0);
	const color_t purple = encodeRGB(31, 0, 31);
	
	init_graphics();
	
	// Draw instructions
	draw_text(0, 0, "w/a/s/d: Move your paintbrush", white);
	draw_text(0, 17, "x/I/o: Change the shape of your brush", white);
	draw_text(0, 34, "Q: Draw a rectangle", white);
	draw_text(0, 51, "r/g/b/B/W/p: Change the colour", white);
	draw_text(0, 68, "Colours cycle between red/green/blue/black/white/purple", white);
	draw_text(0, 85, "q: Quit the application", white);

	// Set up brush coordinates and colour
	char key;
	char *brush = "x";
	color_t colour;
	colour = white;
	int x = (640-20)/2;
	int y = (480-20)/2;

	// Read user input and loop
	do
	{
		key = getkey();
		if(key == 'w') y-=4;
		else if(key == 's') y+=4;
		else if(key == 'a') x-=4;
		else if(key == 'd') x+=4;
		else if(key == 'r') colour = red;
		else if(key == 'g') colour = green;
		else if(key == 'b') colour = blue;
		else if(key == 'B') colour = black;
		else if(key == 'W') colour = white;
		else if(key == 'p') colour = purple;
		else if(key == 'x') brush = "x";
		else if(key == 'o') brush = "o";
		else if(key == 'I') brush = "I";
		else if(key == 'Q') draw_rect(x, y, 20, 20, colour);
		
		//draw the brush stroke
		draw_text(x, y, brush, colour);
		sleep_ms(10);
	} while(key != 'q');
	exit_graphics();
	return 0;
}

/*
Changes the given colour
*/
void changeColour(color_t *colourPtr, color_t red, color_t green, color_t blue, color_t purple, color_t black, color_t white) {
	*colourPtr;
	if (*colourPtr == white)
		*colourPtr = red;
	if (*colourPtr == red)
		*colourPtr = green;
	if (*colourPtr == green)
		*colourPtr = blue;
	if (*colourPtr == blue)
		*colourPtr = purple;
	if (*colourPtr == purple)
		*colourPtr = black;
	if (*colourPtr == black)
		*colourPtr = white;
}
