/*
 *	author: Oliver Lester
 *	description: This program will act as a basic text message program. It does so through two syscalls made 
 *		specifically for the program. One that sends messages and another that gets them. This program supports
 *		multi users. Meaning one can send a message to another user, and that user will be able to recieve the
 *		message on their side.
 *	usage: osmsg -r (to recieve messages) or osmsg -r [recipient] [message]
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

/**
 * This function a wrapper function for the newly made syscall that sends messages. It takes in the necessary
 * 	parameters and calls the syscall. It then returns what the syscall returns.
 * Ths function takes in three strings as parameters. The first representing the person that will recieve a message.
 * 	The next will be the message that will be sent. Ant the last will be the person that is sending the message.  
 */
int send_msg(char *to, char *msg, char *from) {
	int ret;
	ret = syscall(443, to, msg, from);
	return ret;
}

/**
 * This function a wrapper function for the newly made syscall that gets messages. It takes in the necessary
 * 	parameters and calls the syscall. It then returns what the syscall returns. 
 * This function takes in three strings as parameters. The first representing the person that wants to recieve their
 *  	messages. The next will be a message that was sent to the person. And the last will be the person that is
 *  	sent the message.
 */
int get_msg(char *to, char *msg, char *from) {
	int ret;
	ret = syscall(444, to, msg, from);
	return ret;
}

/**
 * The main function takes in command line arguments. It decides whether the user wants to send or recieve messages. 
 * 	Performing the command given. It also has the has backups for ill formed commands. It will return a code upon
 * 	success or failure.
 */
void main(int argc, char *argv[]) {
	if (argc == 2) {
		if (argv[1][0] != '-' || argv[1][1] != 'r') {
			fprintf(stderr, "Get message command must be in form of [comm] -r\n");
			exit(EXIT_FAILURE);
		}
		int ret;
		char msg[512];
		char user[256];
	
		while (1) {
			printf("hh");
			ret = get_msg(getenv("USER"), msg, user);
			if (ret < 0) {
				fprintf(stderr, "Error getting message\n");
				exit(EXIT_FAILURE);
			}
			printf("%s said: \"%s\"\n", msg, user);
			if (ret == 0) {
				printf("All message recieved\n");
				exit(EXIT_SUCCESS);
			}
		}
	} else if (argc == 4) {
		if (argv[1][0] != '-' || argv[1][1] != 's') {
			fprintf(stderr, "Send message command must be in form of [comm] -s to \"message\"\n");
			exit(EXIT_FAILURE);
		}
		int ret;
		ret = send_msg(argv[2], argv[3], getenv("USER"));
		if (ret < 0) {
			fprintf(stderr, "Error sending message\n");
			exit(EXIT_FAILURE);
		}
		printf("Message successfully sent\n");
		exit(EXIT_SUCCESS);
	} else {
		fprintf(stderr, "Program must be in form of [comm] -r or [comm] -s to \"message\"\n");
		exit(EXIT_FAILURE);
	}	
}
