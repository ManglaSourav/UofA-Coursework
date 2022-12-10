/*
 * Author: Orlando Rodriguez
 * CSC 452
 * Project 2
 * 02/20/2022
 * OSMSG program that uses a syscall to send a message to a user
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

int send_msg(char*, char*, char*);
int get_msg(char*, char*, char*);

/*
 * Main function code
 * Calls the syscall to send/recieve messages
 */
int main(int argc, char* argv[]) {
	//printf("%d\n", argc);
	// Sending message
	if (argc == 4) {
		if (strcmp(argv[1], "-s") != 0) {
			fprintf(stderr, "Invalid argument: %s\n", argv[1]);
			return 1;
		}
		send_msg(argv[2], argv[3], getenv("USER"));
		return 0;
	}

	// Receiving messages
	else if (argc == 2) {
		if (strcmp(argv[1], "-r") != 0) {
			fprintf(stderr, "Invalid argument: %s\n", argv[1]);
			return 1;
		}
		char* message = (char*) calloc(100, sizeof(char));
		char* from = (char*) calloc(64, sizeof(char));
		get_msg(getenv("USER"), message, from);
		free(message);
		free(from);
		return 0;
	} 
	else {
		fprintf(stderr, "Invalid input\n");
	}
}

/*
 * Handles sending a message to a user
 */
int send_msg(char *to, char *txt, char *from) {
	//printf("Sending message from %s to %s\n", from, to);
	syscall(443, to, txt, from);
	return 0;
}

/*
 * Handles sending a getting messages for a user
 */
int get_msg(char *to, char *txt, char *from) {
	//printf("Retrieving messages for %s\n", to);
	while (syscall(444, to, txt, from) >= 0) 
		printf("%s said: \"%s\"\n", from, txt);
	return 0;
}













