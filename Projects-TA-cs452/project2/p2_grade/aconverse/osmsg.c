/*
 * Author: Amber Charlotte Converse
 * File: osmsg.c
 * Description: This file implements an inter-user messaging
 * 	system using two new system calls, sys_csc452_send_msg()
 * 	and sys_csc452_get_msg(). The messages are stored in the
 * 	kernel by the system calls. Messages are sent and received
 * 	using command-line arguments.
 *
 * Limitations: Usernames can only be 256 characters long and
 * 	messages can only be 512 characters long. Usernames and
 * 	messages longer than this will NOT cause an error, but will
 * 	be truncated to prevent buffer overflow. 	
 *
 * Commands:
 * 	Send a message: osmsg -s [recipient user] [message]
 * 	Receive messages: osmsg -r
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * This is a helper function to make using the system call more
 * intuitive. This function makes the system call to add a
 * message to the queue.
 *
 * PARAM:
 * to (char*): the recipient of the message
 * msg (char*): the message to send
 * from (char*): the sender of the message (current user)
 *
 * RETURN:
 * int: 0 if successful, -1 if error
 */
int send_msg(char *to, char *msg, char *from) {
	return syscall(443, to, msg, from);
}

/*
 * This is a helper function to make using the system call more
 * intuitive. This function returns one message for the given
 * user using the msg and from pointers.
 *
 * PARAM:
 * to (char*): the recipient of the message (current user)
 * msg (char*): a pointer to a char array with at least 513 bytes
 * from (char*): a pointer to a char array with at least 257 bytes
 *
 * RETURN:
 * int: 0 if no more messages to read, 1 if more messages, -2 if
 * 	there are no messages for this user
 */
int get_msg(char *to, char *msg, char *from) {
	return syscall(444, to, msg, from);
}

int main(int argc, char** argv) {
	if (argc == 1) {
		fprintf(stderr, "ERROR: Please include a command-type "
				"option (-s or -r).\n");
		return 1;
	}

	if (argc == 2 && strcmp(argv[1], "-r") == 0) { // read messages
	
		char* to = getenv("USER");

		char msg[513]; // messages must be less than 512 chars long
		char from[257]; // users must be less than 256 chars long
		int more_messages = get_msg(to, msg, from);
		if (more_messages < 0) {
			printf("You have no messages.\n");
			return 0;
		}
		printf("%s said: \"%s\"\n", from, msg);
		while (more_messages > 0) {
			more_messages = get_msg(to, msg, from);
			printf("%s said: \"%s\"\n", from, msg);
		}

	} else if (argc == 2 && strcmp(argv[1], "-s") == 0) { // missing recipient
		fprintf(stderr, "ERROR: Please include a recipient for your "
				"message.\n");
		return 1;
	} else if (argc == 3 && strcmp(argv[1], "-s") == 0) { // missing message
		fprintf(stderr, "ERROR: Please include a message.\n");
		return 1;
	} else if (argc > 3 && strcmp(argv[1], "-s") == 0) { // send message
		char* to = argv[2];
		char* msg = argv[3];
		char* from = getenv("USER");
		if (send_msg(to, msg, from) == 0) {
			printf("Message successfully sent.\n");
			if (argc > 4) {
				printf("Note: Your command had more than 4 "
				       "arguments. Make sure your meessage is "
				       "in double-quotes so it is treated as one "
				       "argument. Additional arguments were "
				       "ignored.\n");
			}
			return 0;
		} else {
			fprintf(stderr, "ERROR: There was an issue sending your "
					"message.\n");
			return 1;
		}
	} else { // Invalid format
		fprintf(stderr, "ERROR: Invalid command format.\n");
		return 1;
	}

	return 0;
}
