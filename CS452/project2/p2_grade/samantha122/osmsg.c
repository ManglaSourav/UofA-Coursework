/* File: osmsg.c
 * Author: Samantha Mathis
 * Purpose: A Userspace application where users can send and receive short messages
 * via two new system calls.
 */

#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
 * PURPOSE: This method sends a messages to a user. By using a syscall
 * to the kernel
 * @param to, which contains the recipient of the message
 * @param msg, which contains the message 
 * @param from, which contains the sender of the message 
 **/ 
int send_msg(char *to, char *msg, char *from){
	int result = syscall(443, to, msg, from);
	return result;
}

/**
 * PURPOSE: This method grabs the unread messages that the user has. By using a syscall
 * to the kernel
 * @param to, which contains the recipient of the message
 * @param msg, which contains the message 
 * @param from, which contains the sender of the message 
 **/ 
int get_msg(char *to, char *msg, char *from){
	int result = syscall(444, to, msg, from);
	return result;
}

int main(int argc, char *argv[]){
	char *to;
	char *msg;
	char *from;
	//Checks to see if the user typed in a command
	if (argc >= 1){
		//Checks if the user is trying to read the messages
		if (strcmp(argv[1], "-r")==0){
			to = getenv("USER");
			char sender[64] = "";
			char message[1028] = "";
			int result = get_msg(to, message, sender);
			//Tries to read messages when there are none
			if (result == -1){
				printf("No New Messages\n");
			}else{
				printf("%s said: \"%s\"\n", sender, message);
			}
			//If there is more than 1 message to be read
			while (result == 1){
				result = get_msg(to, message, sender);
				printf("%s said: \"%s\"\n", sender, message);
			}
			
		//Checks if the user is trying to send a message
		}else if (strcmp(argv[1], "-s")==0){
			if (argc >= 3){
				char strmsg[1028] = "";
				int i;
				//Concatenates the words from the console to create the message 
				for (i = 3; i < argc;i++){
					strcat(strmsg, argv[i]);
				}	
				to = argv[2];
				msg = strmsg;
				from = getenv("USER");
				int result = send_msg(to, msg, from);
				if (result != 0){
					printf("Message was not sent\n");
				}
			}else{
				printf("Not Enough Arguments for -s\n");
			}
		}
	}else{
		printf("Please provide an arugment -s or -r\n");
	
	}
}
