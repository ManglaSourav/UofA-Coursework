/**
 * Author: Peter Bedrick
 * File: osmsg.c
 * Project 2: Syscalls
 * Purpose: Handles syscalls to send and recieve messages through the os between users
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

// calls get_msg syscall
int get_msg(char* to, char* msg, char* from) {
	return syscall(444, to, msg, from);
}

// calls send_msg syscall
int send_msg(char* to, char* msg, char* from) {
	return syscall(443, to, msg, from);
}

// handles main functionality of sending and recieving messages through os
int main(int argc, char **argv) {
	if(argc != 2 && argc != 4) {
		fprintf(stderr, "Invalid Parameters\n");
		return -1;
	}
	if(strcmp(argv[1], "-r") == 0) {
		// read all messages from user
		int more = 1;
		char msg[255];
		char from[255];
		while(more) {
			more = get_msg(getenv("LOGNAME"), msg, from);
			if(more == -1) {
				printf("No messages to read\n");
				break;
			}
			printf("%s said: \"%s\"\n", from, msg);
		}
	} else if(strcmp(argv[1], "-s") == 0) {
		// send a message
		if(argc == 2) {
			fprintf(stderr, "Invalid Parameters\n");
			return -1;
		}
		// send message argv[3] to argv[2] from current user
		send_msg(argv[2], argv[3], getenv("LOGNAME"));
	}
	return 0;
}
