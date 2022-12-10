/*
 * This program implements the messaging app
 * Author: Jesse Gomez
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
 * Wrapper class to send msgs. All three arguments should be filled.
 */
int send_msg(char *to, char *msg, char *from) {
	return syscall(443, to, msg, from);
}

/*
 * Wrapper class to get msgs, expects three arguments. msg and from should have
 * space allocated for them already;
 */
int get_msg(char *to, char *msg, char *from) {
	return syscall(444, to, msg, from);
}

int main(int argc, char **argv) {
	char *sendtag = "-s";
	char *gettag = "-r";
	char *username = getenv("USER");
	char gotMsg[128];
	char gotFrom[128];
	if (argc == 2 && strcmp(argv[1], gettag) == 0) {
		int keepChecking = get_msg(username, gotMsg, gotFrom);
		if (keepChecking != -1) {
			printf("Here are your messages %s:\n", username);
			printf("%s said: %s\n", gotFrom, gotMsg);
		} else {
			printf("Could not find any messages for %s\n", username);
		}
		while (keepChecking == 1) {
			keepChecking = get_msg(username, gotMsg, gotFrom);
			printf("%s said: %s\n", gotFrom, gotMsg);
		}
	} else if (argc == 4 && strcmp(argv[1], sendtag) == 0) {
		send_msg(argv[2], argv[3], username);
	} else {
		printf("Incorrect number of arguments passed: %d\n", argc-1);
	}
}
