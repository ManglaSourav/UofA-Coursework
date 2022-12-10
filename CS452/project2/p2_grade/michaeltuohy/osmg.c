#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/*
 * File: osmg.c
 * Author: Michael Tuohy
 * Class: CSc452
 * Project: Project 2
 * Description: This is meant to be a driver program for use with two new syscalls
 * that have been added to the kernel: csc452_get_msg and csc452_send_msg.
 * When given command line arguments, this program will send a message to someone else with
 * the "-s" flag, followed by the user you would like to send it to and the message itself.
 * Do note that if either your or the username you sent is more than 25 characters, or your
 * message is more than 50 characters, the message will not be sent. 
 */

// Wrapper method for doing the send_msg syscall
int send_msg(char *to, char*msg, char *from) {
	return syscall(443, to, msg, from);
}

// Wrapper method for doing the get_msg syscall
int read_msg(char *to, char *msg, char *from) {
	return syscall(444, to, msg, from);
}


/*
 * This method will determine which user called it, then 
 * repeatedly try to read messages with read_msg, as well
 * as print debug information for the user should things fail
 */
int read_messages() {
	int i;
	char *to;
	char from[25], msg[50];
	int success;
	to = getenv("USER");


	i = 1;
	while(i) {
		success = read_msg(to, msg, from);
		if(success == 0) {
			printf("%s said: %s\n", from, msg);
		} else if(success == -1 ){
			printf("End of messages\n\n");
			i = 0;
		} else {
			printf("Message failed to be read, terminating\n");
		}
	}

	return 0;
}

/*
 * This method will take two strings representing who the user
 * wishes to send a message to as well as the msg itself.
 * It will also try to catch any errors that the syscall
 * may throw at it
 */
int send_message(char *to, char *msg) {
	char *from = getenv("USER");
	int success = send_msg(to, msg, from);
	
	if(success == 0) {
		printf("Message sent\n");
	} else if(success == -2) {
		printf("Message creation failed, terminating\n");
		return -1;
	} else if(success == -3) {
		printf("Username too large, please try shortening your username\n");
		return -1;
	} else if(success == -4) {
		printf("Recipient username too large, please try a different user\n");
		return -1;
	} else if(success == -5) {
		printf("Message too long, please try shortening your message\n");
		return -1;
	} else {
		printf("Error code: %d\n", success);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[]) {
	
	// As always, we begin by checking the arguments passed to the driver program.

	if(argc == 1) {
		printf("To few arguments, read documentation for instructions\n");
		return -1;
	} else if(argc == 2) {
		if(strcmp(argv[1], "-r") == 0) {
			read_messages();
		} else {
			printf("Did not correctly read messages, use -r to read messages\n");
			return -1;
		}
	} else if(argc == 4) {
		if(strcmp(argv[1], "-s") == 0) {
			send_message(argv[2], argv[3]);
		} else {
			printf("Did not properly send message, use -s to send a message\n");
			return -1;
		}
	} else {
		printf("Incorrect arguments, see documentation\n");
		return -1;
	}
	return 0;
}
