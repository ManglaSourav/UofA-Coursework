/*
 * Author: Molly Opheim
 * File: osmsg.c
 * Project: CSc 452 Project 2
 * Purpose: This file calls the syscalls sys_csc452_send_msg and sys_csc452_get_msg
 * that were created as a part of this project in sys.c
 */
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int send_msg(char*, char*, char*);
int get_msg(char*);


/**
* main serves as the driver for this program, it receives user input 
* to determine whether a message should be sent or read for the user
*/
int main(int argc, char **argv) {
	char * message;
	char * osmsg = "osmsg";
	char * send = "-s";
	char * read = "-r\n";
	char * from = "root";
	char * line = NULL;
	char * toUser;
	char * sendOrRead;
	char * curUser;
	size_t sz = 0;

	while(getline(&line, &sz, stdin) > 0) {
		char * noWhites  = strtok(line, " ");
		// getting the current user
		curUser = getenv("USER");
		while(noWhites != NULL) {
			if(strcmp(noWhites, osmsg) == 0) {
				sendOrRead = strtok(NULL, " ");
				// sending a message
				if(strcmp(sendOrRead, send) == 0) {
					toUser = strtok(NULL, " ");
					message = strtok(NULL, " ");
					send_msg(toUser, message, curUser);
				// getting/reading messages
				}
				if(strcmp(sendOrRead, read) == 0) {
					get_msg(curUser);
				}
			}
			noWhites = strtok(NULL, " ");
		}


		free(line);
	}
}

/*
* Input: This function takes in three character pointer 
* 1. who the message is for
* 2. what the message is
* 3. who the message is from
* Output: 0 for success, and -1 for error
* This function really just uses the syscall sys_csc452_send_msg 
* to send a message
*/ 
int send_msg(char *to, char *msg, char *from) {
	syscall(443, to, msg, from);
}

/*
* Input: who the message is for
* Output: 1 if there are messages for this person, 0 if there is nothing
* else for this person, and negative if there was an error
* this function just uses the syscall sys_csc452_get_msg to read a message
* for this person
*/
int get_msg(char *to) {
	char *fromName = malloc(sizeof(char) * 64);
	char *content = malloc(sizeof(char) * 256);
	int retVal = syscall(444, to, content, fromName);
	if(retVal >= 0) {
		printf("%s said: \"%s\"\n", fromName, content);
	}
}
