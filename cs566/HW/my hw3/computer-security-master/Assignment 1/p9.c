#include <stdio.h>
void foo(){
int i=0;
while(i<10){
	printf("%d, ", i);
	i++;	
}
}

int main(int argc, char*argv[]){
foo();
}
