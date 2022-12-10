/*
* Author: Ryan Alterman
* This program is used to implement two syscalls
* added for this assignment.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

int send_msg (char* to, char* msg, char* from)
{
	return syscall(443, to, msg, from);
}

int get_msg (char* to, char* msg, char* from)
{
	return syscall(444, to, msg, from);
}

int main (int argc, char** argv)
{
	if(argc < 4)
	{
		printf("An incorrect number of arguments passed. Make sure to specify the mode, recipient, and message\n");
		return -1;
	}

	// Send a message
	if(strcmp(argv[1], "-s") ==  0)
	{
		send_msg(argv[2], argv[3], getenv("USER"));
	}
	// Retrieve messages
	else if(strcmp(argv[1], "-r") == 0)
	{
		char* msg;
		char* from;
		while(get_msg(getenv("USER"), msg, from))
		{
			printf("%s said: %s\n", from, msg);
		}
	}
	// An unrecognized option has been passed
	else
	{
		printf("An invalid option has been passed in. Please try again with -s or -r\n");
		return -2;
	}

	return 0;
}
