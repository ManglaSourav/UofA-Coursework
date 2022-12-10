 /* File: osmsg.c
 *
 * Author: Zachary Taylor
 * NetID: ztaylor
 * Class: CSC 452
 * Assignment: P2
 *
 * Creates a messanger program using the syscalls that I made to save messages
 *	 to kernel and retrive them.  Only the user a message is sent to can
 *	 retrive them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
/* Purpose: a wrapper for our sending syscall
 *	
 * Params: to, msg, from are all char pointers to be sent to the syscall
 * Return: a 0 on success, -1 on failure
 */
int send_msg(char *to, char *msg, char *from){
	return syscall(443,to, msg, from);
}
/* Purpose: a wrapper for our getter syscall
 *	
 * Params: to, msg, from are all char pointers to be sent to the syscall
 * Return: a 0 if there are no more messages, -1 on failure or if there were
 *	 never any messages and 1 if there are more messages for the user
 */
int get_msg(char *to, char *msg, char *from){
	return syscall(444,to, msg, from);
}

int main(int argc, char **argv){
	char* msg = (char*) malloc(sizeof(char)*1000);
	char* from = (char*) malloc(sizeof(char)*64);
	int retval = 0;
	int get = 1;

	if(strcmp(argv[1], "-s") == 0){
		retval = send_msg(argv[2], argv[3], getenv("USER"));
	}else if(strcmp(argv[1], "-r") == 0){
		while(get_msg(getenv("USER"), msg, from) >= 0){
			printf("%s: %s\n", from, msg);
		}
		printf("no more messages");
	}
	free(msg);
	free(from);
	return retval;
}
	