/**
 * @file osmsg.c
 * @author Luke Broadfoot (lucasxavier@email.arizona.edu)
 * @brief a simple driver for the 2 new syscalls added for this project
 *        I set the character length of a message to 140 characters like a tweet
 * @version 1.0
 * @date 2022-02-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

int send_msg(char *to, char *msg, char *from) {
	return syscall(443, to, msg, from);
}

int get_msg(char *to, char *msg, char *from) {
	return syscall(444, to, msg, from);
}

int main(int argc, char *args[]) {
	char to[33];
	char from[33];
	char message[141];
	// makes sure the program is called correctly
	if (!(argc == 2 || argc == 4)) {
		fprintf(stderr, "invalid parameter count\n");
		return -1;
	}
	// grabs the user's username
	strcpy(to, getenv("USER"));
	if (argc == 2) {
		// -r is the only valid flag for 2 arguments
		if (strcmp(args[1], "-r") == 0) {
			// get_msg returns 1 if it just read a message, else 0
			while (get_msg(to, message, from) == 1) {
				printf("%s said: \"%s\"\n", from, message);
			}
		} else {
			fprintf(stderr, "invalid flag\n");
			return -1;
		}
	} else {
		// -s is the only valid flag for 4 arguments
		if (strcmp(args[1], "-s") == 0) {
			// send_msg returns 0 if there was no problem, else returns a negative number for an error
			if (send_msg(args[2], args[3], to) == 0) {
				return 0;
			} else {
				fprintf(stderr, "could not send message\n");
			}
		} else {
			fprintf(stderr, "invalid flag\n");
			return -1;
		}
	}
	return 0;
}
