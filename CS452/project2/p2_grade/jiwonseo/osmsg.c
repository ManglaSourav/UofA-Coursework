
/*
Jiwon Seo
Project 2 Syscall
osmsg.c file test the syscall by calling sendmsg and get msg function .
Date: 2/20/2022
SPRING 2022 CSC352
*/
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
This function is to implement the send syscall
you call this function to call 443 syscall
*/
int send_msg(char *to, char *msg, char *from){
	syscall(443, to, msg, from);
}
/*
This function is to impelemnt the get syscall
You call this function to call 444 syscall
*/
int get_msg(char *to, char *msg, char *from){
	msg = (char*)malloc(100);
	from = (char*)malloc(100);
	syscall(444, getenv("USER"), msg, from);
	printf("%s said: %s\n", from, msg);

}

/*
Main method actuallyy call get the command line arguemnts and send out the informatio. 
*/
int main(int argc, char *argv[]){
	char *from;
	char *msg;
	if(argc==4 && strcmp(argv[1],"-s")==0){
		printf("The message is being sent\n");
		//printf("The message is %s\n",argv[3]);
		send_msg(argv[2], argv[3], getenv("USER"));
		return 0;
	}else if(argc==2 && strcmp(argv[1],"-r")==0){
		printf("waiting for message\n");

		get_msg(getenv("USER"), msg, from);

		while(get_msg(getenv("USER"),msg,from)==1){
			get_msg(argv[2], msg, from);
		}
		return 0;
	}else {
		printf("Error in command argument.\n");
		return -1;
	}
}

