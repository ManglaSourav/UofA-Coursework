/*
 * File: osmsg.c
 * Author: Gavin Vogt
 * This program allows a user to send and receive short text messages
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

/*
 * Gets the current user's username from the environment variables
 */
static char *get_username() {
	return getenv("USER");
}

/*
 * Reads all messages to the current user and prints them out
 */
static void read_messages() {
	const char *to = get_username();
	char msg[101];
	char from[33];
	int more_to_read;
	do {
		// Read the next message to this user
		more_to_read = syscall(444, to, msg, from);
		if (more_to_read < 0) {
			// No messages to this user
			fprintf(stderr, "Failed to load messages.\n");
			return;
		} else {
			// Print out the message
			printf("%s said: \"%s\"\n", from, msg);
		}
	} while (more_to_read == 1);
}

/*
 * Sends a message to the given username
 * Returns 0 on success and 1 on failure.
 */
static int send_message(const char *to, const char *msg) {
	// Attempt to send the message
	const char *from = get_username();
	int ret = syscall(443, to, msg, from);
	if (ret == 0) {
		printf("Message sent successfully.\n");
		return 0;
	} else {
		fprintf(stderr, "Failed to send message.\n");
		return 1;
	}
}

int main(int argc, char *argv[]) {
	if (argc == 2 && strcmp(argv[1], "-r") == 0) {
		// Read messages
		read_messages();
	} else if (argc == 4 && strcmp(argv[1], "-s") == 0) {
		// Send a message
		return send_message(argv[2], argv[3]);
	} else {
		fprintf(stderr, "Invalid command.\n");
		return 1;
	}

	return 0;
}
