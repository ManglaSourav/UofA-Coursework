#include "graphics.h"

int main() {
	init_graphics();
	clear_screen();
	int p;
	for (p=0; p<10; p++) {
		draw_rect(2*p*p,2*p*p, 5*p, 5*p,0xFFFF00);
		sleep_ms(60);
	}
	
	for (p=0; p<10; p++) {
		draw_rect((640-(2*p*p)), (480-(2*p*p)), 5*p, 5*p, 
		0xFF00FF);
		sleep_ms(60);
	}
	draw_rect(320, 240, 50, 50, 0xFFFFFF);
	exit_graphics();
	return 0;
}
