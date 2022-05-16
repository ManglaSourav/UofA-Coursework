/* File: osmsg.c
 * Author: Chris Herrera
 * Purpose: Use our newly created syscalls to implement a simple messaging system
 */
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>

// Send a message, msg, to the name specified by to, from the current user.
int send_msg(char *to, char *msg, char *from) {
	return syscall(443, to, msg, from);
}


// Get a single message from the linked list stored in the kernel
int get_msg(char *recipient) {
	int retval;
	char msg[256];
	char from[64];
	retval = syscall(444, recipient, &msg, &from);
	if (retval > 0) {
		printf("%s said: %s\n", from, msg);
	}
	return retval;
}


// Parse the given arguments and make syscalls appropriately
int main( int argc, char *argv[] ) {
	char *to;
	char *from;
	char *msg;
	
	if (argc == 4) {
		if ((strcmp(argv[1], "-s") != 0)) {
			fprintf(stderr, "Invalid arguments\n");
			exit(0);
		}
		to = argv[2];
		msg = argv[3];
		from = getenv("USER");
		int sent = send_msg(to, msg, from);	
		if (sent != 0) {	// Error checking
			fprintf(stderr, "An error occured when attempting to send a message\n Perhaps the username or content was too long\n");
			exit(-1);
		}

	}

	if (argc > 4) { // Error checking
		fprintf(stderr, "Too many arguments\n");
		exit(0);
	}

	if (argc == 2) {

		if ((strcmp(argv[1], "-r") != 0)) { // Error checking
			fprintf(stderr, "Invalid argument\n");
			exit(0);
		}
		
		// Continue getting messages until there aren't any left
		int callAgain = get_msg(getenv("USER"));		
		while (callAgain == 1) {
			callAgain = get_msg(getenv("USER"));
		}

	}
	
	return 0;
}


