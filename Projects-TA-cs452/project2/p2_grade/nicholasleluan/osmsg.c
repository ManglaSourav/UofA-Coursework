
/*
Author: Nicholas Leluan
CSC 452 Dr. Misurda
OS Messenger ::
This program will use two custom made system calls that are stored in sys.c. These system calls will
aid in sending small messages back and forth through the kernel. We will accomplish this by using
this program to interpert a command line call either requesting to read or send a message to another
user.
A user will be defined as another username created on the current system. We will assume that the 
default user is 'root' and any users used are other users that reside on the system.
*
/

/*
  HEADERS -  no header file was requested for this project
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <errno.h>

int send_msg(char* to, char *msg, char* from);
int get_msg();

int MSG_BUFFER = 100;
int FROM_BUFFER = 25;

int main(int argc, char **argv){
	// check to see if we even have arguments
	if(argc <= 1){
		fprintf(stderr,"No arguments were supplied.\n");
		return -1;
	}
	char *mode_flag = argv[1];
	//RECEIVING:
	if(strcmp("-r",mode_flag) == 0){
		return get_msg();
	}
	// SENDING
	else if(strcmp("-s",mode_flag) == 0){
		// not the correct amount of arguments
		if(argc != 4){
			fprintf(stderr,"Not enough arguments given to send message\n");
			return -1;
		}
		char *to = argv[2]; // who the message is being sent to
		char *msg = argv[3]; // the message
		char *from = getenv("USER"); // who the message is from (will be current user)
		return send_msg(to,msg,from);
	}
	return 0;
}
/*
This function is used to abstract a little away from using a sytem call to make the main() 
function a little more readable. What this function does is call syscall 443, which is the 
system call set up for "sending" messages to a specified user.
This function does not check if the intended recipient is a valid user on the system
The only error checking this does is verifies that calling of the system call does not cause
an error in /kernel/sys.c
*/
int send_msg(char *to, char *msg, char *from){
	if(syscall(443,to,msg,from) < 0){
		fprintf(stderr,"There was an error sending your message.");
		return -1;
	}
	printf("Thank you for using OSMSG, %s!\nMessage to %s was sent successfully!\n",from,to);
	return 0;
}
/*
This function is used to abstract away from the main method the inner system calls that this 
program needs to do. What this function does is call system call 444, which is the system call 
that recieves any messages for the current user of the system.
So long as there are messages left for the user, they will print out to standard console.
As soon as the inner workings of the system call no longer have a message for the current
system user, this function will complete.
*/
int get_msg(){
	char *from = malloc(sizeof(char)*MSG_BUFFER);
	char *msg = malloc(sizeof(char)*MSG_BUFFER);
	int flag = 0; // used for user UI at end
	while(syscall(444,getenv("USER"),msg,from) >= 0){
		printf("%s said: \"%s\"\n",from,msg);
		flag = 1;
	}
	if(flag) printf(">> No more messages!\n"); // there was atleast 1 message to print
	else printf(">> Your inbox is empty!\n"); // there were no messages for user when called
	return 0;
}
