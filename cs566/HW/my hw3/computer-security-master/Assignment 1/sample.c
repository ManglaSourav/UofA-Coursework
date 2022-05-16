unsigned int countSetBits(char n) 
{ 
    unsigned int count0 = 0, count1 = 0; 
   
    while (n) 
    { 
        count1+=n&1;
        if(0 == (n&1)) count0++;
        n>>=1;                
    } 
    printf("zeros %d \n", count0);
printf("ones %d \n", count1);
    return 0; 
} 
  
/* Program to test function countSetBits */
int main() 
{ 
    char c = 'z'; 
    countSetBits(c); 
    return 0; 
} 