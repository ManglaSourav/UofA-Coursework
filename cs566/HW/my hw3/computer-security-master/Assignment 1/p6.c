#include <stdio.h>
#include <stdlib.h>
int main(int argc, char* argv[]){
if(argc==1){
printf("Error: no argument\n");
}
else if(argc>=3){
printf("Error: only one argument\n");
}
else{
FILE* file;
file = fopen(argv[1], "r");
if(file != NULL){
char lineByLine[1000];
while(fgets(lineByLine,sizeof(lineByLine),file)!=NULL){
printf("%s\n",lineByLine);
}
fclose(file);
}
else{
printf("Error: can't read\n");
}
}
}
