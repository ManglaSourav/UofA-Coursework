#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// send a message to another user
// to: recipient username, null terminated
// msg: your msg, null terminated
// from: sender's username, null terminated
//
// returns 0 on success, -1 on error
int send_msg(char *to, char *msg, char *from) {
	return syscall(443, to, msg, from);
}

// retrieve the first message in mailbox
// to: mailbox owner's name
// from: address to write in sender's name
// msg: address to write in the message
//
// returns 1 if there's more messages
// returns 0 if there's no more messages
// returns -1 if something went wrong (mailbox empty)
int get_msg(char *to, char *msg, char *from) {
	return syscall(444, to, msg, from);
}

int main(int argc, char **argv) {
	// strings for sender name, recipient name, & message
	char sender[65], recp[65], buff[65];
	bzero(sender, 65); bzero(recp, 65); bzero(buff, 65);

	// user invokes ./osmsg -r
	if (argc == 2 && (strcmp(argv[1], "-r") == 0)) {
		// read from user's mailbox
		strcpy(recp, getenv("USER"));
		int retval = 1;
		while (retval == 1) {
			retval = get_msg(recp, buff, sender);
			if (retval == -1) {
				printf("Couldn't retrieve any messages\n");
				exit(1);
			} else printf("%s said: %s\n", sender, buff);
			bzero(sender, 65);
			bzero(buff, 65);
		}
		exit(0);
	}

	// user invokes ./osmsg -s recipient message
	if (argc == 4 && (strcmp(argv[1], "-s") == 0)) {
		// send a message
		strcpy(sender, getenv("USER"));
		send_msg(argv[2], argv[3], sender);
		exit(0);
	}

	printf("usage: %s -s recipient message\n", argv[0]);
	printf("usage: %s -r\n", argv[0]);
	exit(0);
}
