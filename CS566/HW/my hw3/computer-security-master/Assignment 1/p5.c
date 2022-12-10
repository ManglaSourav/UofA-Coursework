#include <stdio.h>
#include <stdint.h>
int main(){
uint32_t a = 0xaabbccdd;
uint32_t b = 0x11223344;
uint32_t result =  (a&0x55555555)|(b&0xaaaaaaaa);
printf("%x \n",result);
}
