#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
bool xor_bit(bool a , bool b){
bool final = ((a&b)?0:(a|b));
return final;
}
uint32_t xor_word(uint32_t a, uint32_t b){
int result = 0;
int i;
for(i=31;i>=0;i--){
bool b1 = a&(1<<i);
bool b2 = b&(1<<i);
bool xorbit = xor_bit(b1,b2);
result<<=1;
result|=xorbit;
}
return result;
}
int main(){
uint32_t a = 0xaabbccdd;
uint32_t b = 0x11223344;
uint32_t result;
result = xor_word(a,b);
printf("%x \n",result);
return 0;
}
