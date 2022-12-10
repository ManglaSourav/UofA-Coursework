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
			char c;
			int oneCount = 0;
			int zeroCount = 0;
			for(c=fgetc(file);c!=EOF;c=fgetc(file)){
			if(c=='1'){
				oneCount = oneCount+1;
			}
			 if(c=='0'){
				zeroCount = zeroCount+1;
			}
			}
						
			fclose(file);
			printf("Total number of 0:s %d\n",zeroCount);
			printf("Total number of 1:s %d\n",oneCount);
			printf("Total number of 0:s and 1:s in the file is %d\n",zeroCount+oneCount);
		}		

		else{
			printf("Error: can't read\n");
		}
	}
}
