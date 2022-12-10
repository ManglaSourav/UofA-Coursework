/*
 * Filename: osgmsg.c
 * Author: Kaiden Yates
 * Encapsultates two new syscalls that send a message to another user
 * or recieves messages for the user
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
 * Passes params to the csc452_send_msg syscall, and checks for errors
 * Params: to; the user to send the message to
 *	   msg;	the message to send to the other user
 *	   from; the current user as a string
 */
int send_msg(char *to, char *msg, char *from) {
	if (syscall(443, to, msg, from) == -1) {
		fprintf(stderr, "send_msg syscall failed\n");
		exit(-1);
	}
	return 0;
}

/*
 * Using the csc452_get-msg syscall, will read and print out any
 * messages for the current user
 * Parmas: to; the current user
 *         msg_buff; a preallocated buffer to store the message
 *         from_buff; a preallocated buffer to store who sent the message
 */
int get_msg(char *to, char* msg_buff, char* from_buff) {
	int more = syscall(444, to, msg_buff, from_buff);
	size_t msg_len;

	// Checks if the syscall failed
	if (more == -1) {
		fprintf(stderr, "get_msg syscall failed\n");
		exit(-1); 
	}

	// Checks if the syscall stored anything in the first syscall
	msg_len = strlen(msg_buff);
	if (msg_len == 0) {
		printf("No messages to recieve\n"); 
		return 0;
	}
	
	// Prints the results of the syscall
	printf("%s said: %s\n", from_buff, msg_buff);
	
	// Will read and print each message to the current user until
	// the syscall stops returning 1
	while(more == 1) {
		more = syscall(444, to, msg_buff, from_buff);
		printf("%s said: %s\n", from_buff, msg_buff);
	}
	return 0;
}

int main(int argc, char** argv) {
	char* from_buff;
	char* msg_buff;
	char* user;

	// Allocates space for the syscall to store strings into
	from_buff = malloc(32 * sizeof(char));
	msg_buff = malloc(1024 * sizeof(char));
	
	user = getenv("USER");	

	// Handles calls for sending
	if (strcmp(argv[1], "-s") == 0 && argc == 4) {
		send_msg(argv[2], argv[3], user);
	}
	// Handles calls for recieving
	else if (strcmp(argv[1], "-r") == 0 && argc == 2) {
		get_msg(user, msg_buff, from_buff);	

	}
	// Checks for proper arguments 
	else {
		fprintf(stderr, "Invalid arguments\n");
		exit(-1);
	}
	return 0;
}
