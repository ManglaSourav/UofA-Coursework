/*
 * Author: Winston Zeng
 * File: osmsg.c
 * Class: CSC 452, Spring 2021
 * Assignment: Project 2: Syscalls
 * Date: 2/20/22
 * Purpose: Simple interface for passing text messages from one user
 * to another by way of the kernel.
 */

#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Function: message sender, invokes send_msg syscall
 * @param to: recipient user
 * @param msg: message
 * @param from: sender user
 */
int send_msg(char *to, char *msg, char *from) {
	syscall(443, to, msg, from);
}

/*
 * Function: message getter, involes get_msg syscall. Prints off messages as they are found.
 * @param to: recipient user
 * @param msg: message
 * @param from: sender user
 *
 * @return: returns 1 if message found for user and more messages exist,
 * returns 0 if message found for user and mo nore messages exist,
 * returns -1 if no messages have been sent at all.
 */
int get_msg(char *to, char *msg, char *from) {
	int ret = syscall(444, to, msg, from);
	if(ret == 0 || ret == 1) {
		// Message found for user
		printf("%s said: \"%s\"\n", from, msg);
	} else {
		printf("No more messages\n");
	}
	return ret;
}

/*
 * Function: main, handles command line invocation nuances.
 * @param argc: number of command line arguments (2 or 4 are only valid values)
 * @param *argv[]: list of arguments
 */
int main(int argc, char *argv[]) {
	if(argc != 2 && argc != 4) {
		printf("Usage: osmsg -s [intended recipient] message\nOr: osmsg -r\n");
		exit(0);
	} else if(argc == 2) {
		if(strcmp(argv[1], "-r") != 0) {
			printf("Usage: osmsg -r\nTo receive a message, if exists\n");
			exit(0);
		} else {
			char name[32];
			char content[1000];
			// Get all messages for the user
			int ret = get_msg(getenv("USER"), content, name);
			while(ret != -1) {
				ret = get_msg(getenv("USER"), content, name);
			}
		}
	} else if(argc == 4) {
		if(strcmp(argv[1], "-s") != 0) {
			printf("Usage: osmsg -s [intended recipient] message\nTo send a message\n");
			exit(0);
		} else {
			// Send a message
			send_msg(argv[2], argv[3], getenv("USER"));
		}
	}
}
