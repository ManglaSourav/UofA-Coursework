/*
 * Author: Jacob Hurley
 * Assignment: 2
 * Due Date: 2/20/2022
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define _GNU_SOURCE
#include <sys/syscall.h>
#include <unistd.h>

/*
 * This function makes a syscall to send msg
 */
int send_msg(char *to, char* msg, char* from){
	int retval = syscall(443,to,msg,from);
	return retval;
}

/*
 * This function makes a syscall to get msg
 */
int get_msg(char* to, char* msg, char* from){
	int retval = syscall(444,to,msg,from);
	return retval;
}

int main(int argc, char *argv[]){
	char* curUser = getenv("USER"); // get the current user
	if(curUser == NULL){ // if the user doesnt exist somehow, error
		return -1;
	}
	if(strcmp(argv[1],"-s") == 0){ // if the command is to send
		char* sendTo = argv[2];
		char* message = argv[3];
		int retval = send_msg(sendTo, message, curUser);
		return retval;
	}
	else if(strcmp(argv[1], "-r") == 0){ // if the command is to read
		char from[500];
		char message[500];
		for(int i = 0; i<500; i++){ // set every char to null terminator
			message[i] = '\0';
			from[i] = '\0';
		}
		int retval = get_msg(curUser, message, from);
		if(retval == -1){ // error if -1
			printf("Error in getMsg function\n");
		}
		else if(retval == -2){ // -2 means no msgs to be read
			printf("No messages to read currently!\n");
		}
		else{ // otherwise, print message
			printf("%s said: \"%s\"\n",from,message);
		}
		return retval;
	}
	return 0;
}
