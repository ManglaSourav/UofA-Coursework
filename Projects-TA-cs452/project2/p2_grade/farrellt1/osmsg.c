/*
File: osmsg.c
Author: Tristan Farrell
Class: CSC 452
Professor: Dr. Misurda
Purpose: Utilize new syscalls to send and recieve messages
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

/*
send_msg adds a message to the kernel
Param: strings to, msg, and from are added to the message list
*/
int send_msg(char *to, char *msg, char *from){
	return syscall(443, to, msg, from);
}

/*
get_msg gets and removes a message for the current user.
Param: to is the username, msg and from are empty string that
	will be filled with message details if available
*/
int get_msg(char *to, char *msg, char *from){
	return syscall(444, to, msg, from);
}

/*
main handles command line input to either send or get a message
from the kernel. -s command is send. -r command is read
send also has username to send to and the message as arguments.
Param: argc is # of command line arguments
       argv is the list of arguments
*/
int main(int argc, char **argv){
	char *cmd;
	if(argc == 2){
		cmd = argv[1];
		if(strcmp(cmd, "-r")!=0){ //not read command
			return -1;
		}
		int more = 1;
		while(more==1){
			char msg[300]; //to be filled by syscall
			char from[300];
			more = get_msg(getenv("LOGNAME"), msg, from);

			if(more>=0){ // Message Found
				printf("%s said: %s\n", from, msg);
			}else{ // No Message Found
				printf("No Messages. Syscall returned: %d\n",more);
			}
		}
	}else if(argc == 4){
		cmd = argv[1];
		if(strcmp(cmd, "-s")!=0){ // not send command
			return -1;
		}
		char *name = argv[2]; // strings from command line arguments
		char *message = argv[3];
		int x = send_msg(name, message, getenv("LOGNAME"));

		if(x < 0){ // Syscall failed
			printf("Unable to send message\n");
		}
	}else{ // Incorrect command line arguments
		return -1;
	}
	return 0;
}
