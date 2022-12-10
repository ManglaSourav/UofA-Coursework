/**
 * @author Carter Boyd
 *
 * CSc_452, Spring 22
 *
 * this program serves for two purposes, the first argument that the user created will determine
 * whether the program will send to a user or receive messages sent to the current user.
 *
 * If -s then the program will require the next two arguments after to be where the message will be
 * sent and the message, after which it will send a syscall
 *
 * if -r there will be no more parameters needed and will proceed to print out every message sent
 * to the current user in the order of which was sent most recent
 *
 * any other character will print out the usage instructions
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

/**
 * creates a syscall to send a message
 * @param to who the message is being sent to
 * @param msg the message that is being sent
 * @param from who the message was from
 * @return the return value of the syscall
 */
int send_msg(char *to, char *msg, char *from) {
	int ret = syscall(443, to, msg, from);
	if (ret == -1) {
		fprintf(stderr, "error: send message failed, return was %d\n", ret);
		exit(EXIT_FAILURE);
	}
	return ret;
}

/**
 * creates a syscall to receive a message
 * @param to who the message is being sent to
 * @param msg the message that is being sent
 * @param from who the message was from
 * @return the return value of the syscall
 */
int get_msg(char *to, char *msg, char *from) {
	int ret = syscall(444, to, msg, from);
	if (ret == -1) {
		fprintf(stderr, "error: get message failed, return was %d\n", ret);
		exit(EXIT_FAILURE);
	}
	return ret;
}

int main(int argc, char *argv[]) {
	if (argc <= 1) {
		fprintf(stderr, "too few of arguments, usage is -(s or r) name \"the message\"\n");
		return EXIT_FAILURE;
	}
	char instruction = argv[1][1];
	if (instruction == 's') {
		if (argc <= 3) {
			fprintf(stderr, "too few of arguments for send, usage is -(s or r) name \"the message\"\n");
			return EXIT_FAILURE;
		}
		int status = send_msg(argv[2], argv[3], getenv("USER"));
		if (status == 0)
			printf("Message sent\n");
		else
			printf("message sent but syscall came back %d\n", status);
	} else if (instruction == 'r') {
		char from[64], msg[64];
		while (get_msg(getenv("USER"), msg, from) != 1)
			printf("%s said: %s\n", from, msg);
	} else {
		fprintf(stderr, "Instruction was not a valid character, -r for receive & -s for send\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
