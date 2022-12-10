/*
Author: Ember Chan
Filename: osmsg.c
Course: CSC452 Fall21
Purpose: Display and send messages through system calls
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>



/*
Sends the message msg from the current user
to to
*/
void send_message(char *to, char *msg){
	char *from = getenv("USER");
	syscall(443, to, msg, from);
}

/*
Receives and prints out all messages for the current user.
If there are no messages, "No new messages!" will be printed
instead.
*/
void get_messages(){
	char *to = getenv("USER");
	char from[1025];
	char msg[1025];
	int r;
	do {
		r = syscall(444, to, msg, from);
		if(r < 0){
			printf("No new messages!\n");
			return;
		}
		msg[1024] = '\0';
		from[1024] = '\0';
		printf("%s said: %s\n", from, msg);
	} while (r == 1);
}

int main(int argc, char **argv){
	if(argc == 2 && strcmp(argv[1], "-r") == 0){
		//Receive Message
		get_messages();
	} else if (argc == 4 && strcmp(argv[1], "-s") == 0){
		//Sending message
		char *to = argv[2];
		char *msg = argv[3];
		send_message(to, msg);
	} else {
		printf("bad arguments\n");
	}

}
