
/*
	Author: Emilio Santa Cruz
	Description: Serves as a wrapper program to use the introduced
		syscalls of sendMsg and getMsg. Does so by taking input
		from the console to either send a message under a
		different username or receive all messages sent to them.
	Class: CSC 452
	Professor: Dr. Misurda
	OS: Arch Linux 5.12.6
	Usage: ./osmsg -s <Receipant> "<Message>" or ./osmsh -r
	Known bugs: A single send request needs to be made in order to initalize
		the linked list in the kernal and it can not be accessed.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
	Author: Emilio Santa Cruz
	Description: Wrapper to send messages to different users.
	Parameters: to is the receipient of the message, msg is the text
		to be sent, from is the user's name.
	Pre-Condition: A send request is made
	Post-Condition: A message is ready to be receieved by to
	Return: None
*/
void sendMsg(char *to, char *msg, char *from){
	if(syscall(443, to, msg, from) == -1){
		printf("%s\n", "ERROR: sendMsg syscall threw error.");
	}
}

/*
	Author: Emilio Santa Cruz
	Description: Wrapper to receive messages from other users
	Parameters: user is the user's name
	Pre-Condition: A receive request is made
	Post-Condition: Message(s) are received or nothing if they're none
	Return: None
*/
void getMsg(char *user){
	char msg[65] = "";
	char from[33] = "";
	int msgFlag = syscall(444, user, msg, from);
	if(from[0] != 0) {printf("%s said: \"%s\"\n", from, msg);}
	while(msgFlag == 1){	// loop to get multiple messages if they're any
		memset(msg, 0, 65);
		memset(from, 0, 33);
		msgFlag = syscall(444, user, msg, from);
		printf("%s said: \"%s\"\n", from, msg);
	}
}

int main(int argc, char **argv){
	char *user = getenv("USER");
	int flag;

	// handles sending
	if(argc == 4 && strcmp(argv[1], "-s") == 0){
		sendMsg(argv[2], argv[3], user);
	}else if(argc == 2 && strcmp(argv[1], "-r") == 0){ // handles receiving
		getMsg(user);
	}else{	// error case for wrong usage
		printf("USAGE: -s USER MESSAGE or -r\n");
	}

	return 0;
}
