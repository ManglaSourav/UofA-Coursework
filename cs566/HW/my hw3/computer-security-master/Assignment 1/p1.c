#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(){
char* s = "ATTACK AT DAWN";
int len=strlen(s); 
int xor[len];
int i;
//use a loop to xor every byte with 0x42
for(i=0;i<len;i++)
{
xor[i]=(s[i]^0x42);
}
//use a loop to print out every byte (as an integer) of s, each on its own line
for(i=0;i<len;i++){
printf("%c XOR with 0x42 is %d \n",s[i],xor[i]);
}
return 0;
}

