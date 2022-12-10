#include <stdio.h>
#include <stdlib.h>
int main(){
//declare a pointer to a float array
float* numArr;
//allocate the array using malloc
numArr = (float *)malloc(10);
float sum = 0.0;
int i;
//fill in numbers 1.0 to 10.0 using a loop
for(i=0;i<10;i++)
{
numArr[i]=i+1;
}
//use a loop to sum up the array
for(i=0;i<10;i++)
{
sum=sum+numArr[i];
}
//print out the sum
printf("Sum is %0.1f \n",sum);
return 0;
}

