/*
 * Author: Kyle Smith
 * Project 2 System Calls CSC 452
 * Function: This program uses a user implmented system call that allows multiple users
 * transfer simple messages to each other via the OS. Command line arguments are read in
 * with either the send command or read command and messages are either sent or displayed to
 * the user respectively.
*/

#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>

// Syscall wrapper function for newly implemented csc_452_sendmsg
int send_msg(char *to, char *msg, char *from) {
	return syscall(443, to, msg, from);
}

// Syscall wrapper function for newly implemented csc_452_getmsg
int get_msg(char *to, char *msg, char *from) {
	return syscall(444, to, msg, from);
}

int main (int argc, char *argv[]) {
	char read[3] = "-r";
	char send[3] = "-s";
	
	// Check command line args for send or read commmands
	if (argc != 2 && argc != 4) {
		fprintf(stderr, "Incorrect Amount of Args \n");
		return -1;
	}
	else if (argc == 2){
		if (strcmp(argv[1], read) != 0) {
			fprintf(stderr, "Incorrect read command line args \n");
			return -1;
		}
	}
	else {
		if (strcmp(argv[1], send) != 0) {
			fprintf(stderr, "Incorrect send command line args \n");
			return -1;
		}
	}
	char login[33];
	
	// Get our current username
	strcpy(login, getenv("USER"));
	//printf("username:|%s|\n", login);

	if (strcmp(argv[1], read) == 0) {
		// Read Message
		char message[256];
		char from[33];
		int empty = 1;	
		
		//printf("retval: %d \n",get_msg(login, message, from));
		
		// Iterate through our nodes and display to user
		while (get_msg(login, message, from) == 1) { 
			// we return zero once we've fully iterated through our LL queue
			printf("%s said: %s \n", from, message);
			empty = 0;
		}
		if (empty) {
			printf("NO NEW MESSAGES \n");
		}
		return 0;
	}
	else {
		// Send Message

		//printf("retval: %d \n",send_msg(argv[2], argv[3], login));
		
		if (send_msg(argv[2], argv[3], login) == 0) {
			// No issues sending
			return 0;
		}
		else {
			// Problem sending code through our syscall? Shouldn't happen
			fprintf(stderr, "Message won't go thru \n");
			return -1;
		}
	}
}
