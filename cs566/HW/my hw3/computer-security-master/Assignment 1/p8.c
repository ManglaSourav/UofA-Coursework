#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char* argv[]){
int length;
if(argc ==1){
printf("Error: no argument \n");
}else if(argc >= 3){
printf("Error: only one argument \n");
}
else{
	FILE* file;
	char c;
	file = fopen(argv[1],"r");
	if(file!=NULL){
		struct stat buffer;
		int link;
		link = lstat(argv[1], &buffer);
		
		if(S_ISLNK(buffer.st_mode)== 1){
		c = fgetc(file);
		while(c!=EOF){
			printf("%c",c);
			c = fgetc(file);
}fclose(file);
}else{
printf("Error: not a symlink\n");
}
}else { 
	printf("Error: can't read\n");
}
}
}

