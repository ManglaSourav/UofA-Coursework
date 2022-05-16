/*
 * File: osmsg.c
 * Author: Justin Duross
 * Purpose: This program allows a user to send and
 * receive short text messages to other users. Usernames
 * and messages are assumed to be no more than 100 characters
 * plus the null terminating character.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/*
 * Sends the message to the correct user by calling the proper syscall. Passes the to, msg,
 * and from strings to the syscall, then returns the value that the syscall returned.
*/
int send_msg(char *to, char *msg, char *from) {
	int retval = syscall(443, to, msg, from);
	//printf("syscall retval: %d\n", retval);
	return retval;
}

/*
 * Gets the messages that are waiting for the current user. Passes in the current username
 * to the syscall, and msg and from strings that are return values for the read messages.
 * It will call the syscall until no more messages are available to be read for the current
 * user.
*/
int get_msg(char *currUser) {
	char *msg = malloc(sizeof(char) * 101);
	char *from = malloc(sizeof(char) * 101);
	int syscallretval;
	while ( (syscallretval = syscall(444, currUser, msg, from)) ) {
		printf("%s said: %s\n", from, msg);
	}
	if (syscallretval == 0 && strlen(msg) == 0 && strlen(from) == 0) {
		printf("No messages at this time\n");
	}
	else {
		printf("%s said: %s\n", from, msg);
	}

	free(msg);
	free(from);
	return 0;

}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		printf("Must use command line arguments correctly\n");
		printf("  'osmsg -r' to receive messages\n");
		printf("  'osmsg -s username message' to send messages\n");
		exit(1);

	}
	// if user wants to send a message
	else if (strcmp(argv[1], "-s") == 0 && argc == 4) {
		//send message
		//printf("%s\n", argv[3]);
		//printf("sending message\n");
		char *currUser = getenv("USER");
		if (currUser == NULL) {
			printf("Error getting current username\n");
			exit(1);
		}
		int sendret = send_msg(argv[2], argv[3], currUser);
		if (sendret < 0) {
			printf("Error sending message!\n");
			exit(1);
		}
		else {
			printf("Message sent successfully!\n");
		}
	}
	// if user wants to recieve messages
	else if (strcmp(argv[1], "-r") == 0 && argc == 2) {
		//receiving all messages
		//printf("receiving messages\n");
		char *currUser = getenv("USER");
		if (currUser == NULL) {
			printf("Error getting curret username\n");
			exit(1);
		}
		//printf("%s\n", currUser);
		get_msg(currUser);
	}
	else {
		printf("Must use command line arguments correctly\n");
		printf("  'osmsg -r' to receive messages\n");
		printf("  'osmsg -s username message' to send messages\n");
		exit(1);
	}
	return 0;
}
