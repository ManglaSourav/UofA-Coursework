#include <stdio.h>
#include <limits.h>
#include <stdint.h>
union convert{
//declare a 32-bit floating point field
float f;
//declare a 32-bit integer field
uint32_t i;
};
int main(){
union convert conv;
float f = 42.0;
uint32_t i;
conv.f = f;
//convert the bits of f to an integer
//considering INT_MAX = 2147483647 and it's hexadecimal value = 0x80000000 for the computation

// took the help from stackoverflow 
for(i=0x80000000;i;i>>=1)
{
if(i&conv.i) printf("1"); else printf("0");
}
printf("\n%d\n",conv.i);
return 0;
}
