#include<stdio.h>
#include"library.c"
#define BLUE  30
#define GREEN 2016
#define RED   63488
int main(void){
printf("start\n");
sleep_ms(1500);
clear_screen();

init_graphics();
int i=0;
for( i; i<480; i++){
    draw_pixel(i, 50, BLUE );
    draw_pixel(i, 60, GREEN);
    draw_pixel(i, 70, RED  );
}
sleep_ms(100);
for(i=0; i<640; i++){
    draw_pixel(50, i, RED  );
    draw_pixel(60, i, GREEN);
    draw_pixel(70, i, BLUE );
}
sleep_ms(250);

draw_rect(100,100,30,30, RED  );
sleep_ms(250);
draw_rect(110,110,10,10, GREEN);
sleep_ms(250);
draw_rect( 90, 90,50,50, BLUE );

sleep_ms(1000);

fill_circle(200,200, 50, BLUE);

draw_rect(150,150,100,100,RED);

sleep_ms(250);
fill_circle(200,200,20,GREEN);

sleep_ms(250);
fill_circle(400,200,10, GREEN);

clear_screen();
int q=0;
int z=0;
while(getkey()==NULL&&q<600){
    for(q=0; q<480; q++){
        draw_pixel(q,z,GREEN);
    }
    z++;
    sleep_ms(100);
}
clear_screen();

draw_text(10,10, "Testing writing text", RED);
draw_text(50,10, "This is BLUE", BLUE);
sleep_ms(250);
exit_graphics();

printf("done\n");

return 0;

}
