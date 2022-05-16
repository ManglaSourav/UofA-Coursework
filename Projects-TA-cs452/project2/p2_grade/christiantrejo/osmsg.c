











/*
Author: Christian Trejo
Course: CSC452
Assignment: Project 2 - Syscalls
File: osmsg.c
Purpose: Implements the osmsg system using our system calls.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Purpose: Sends a single message.
Parameters:
	to - recipient's username; char pointer
	msg - message to send; char pointer
	from - sender's username; char pointer
Returns:
       <0 - error in sending message
	0 - message sent successfully
*/
int send_msg(char *to, char *msg, char *from){
	int retval = syscall(443, to, msg, from);
	return retval;
}


/*
Purpose: Gets message for recipient.
Parameters:
	to - recipient's username; char pointer
	msg - the message; char pointer
	from - the sender's username; char pointer
Returns:
       <0 - error in retrieving message
	0 - no more messages to deliver
	1 - call function again
*/
int get_msg(char *to, char *msg, char *from){
	int more_mess = syscall(444, to, msg, from);
	return more_mess;
}


/*
Purpose: Send or retrieve messages for the current user.
How to use:
	To send messages:
		osmsg -s recipient_username "message"
	To retrieve message(s):
		osmsg -r
Parameters:
	argc - number of arguments entered on the command line; int
	       2 = get message(s), 4 = send message
	argv - command line arguments; array of char pointers
Returns:
	0 - int signifying end of program
*/
int main(int argc, char *argv[]){

	if(argc < 2){			//Improper input
		printf("Invalid number of arguments (<2).\n");
		return 0;
	}

	//Set up variables for checking what action the user wants
	char *curr_user = getenv("USER");		//Get current username
	char send_com[] = "-s";				//Send message command
	char get_com[]  = "-r";				//Get message command
	int send_mess = strcmp(argv[1], send_com);	//Send message?
	int get_mess  = strcmp(argv[1], get_com);	//Retrieve message?

	int retval;					//Used to check return values
	int stop = 0;					//Used in get message

	//Set up string variables used in get and send
	char *sender;					//Sender of message
	char *message;					//Message
	char *recipient;				//Recipient of message

	//If user wants to send message
	if(send_mess == 0 && argc == 4){

		//For simplicity, give values variable names
		recipient = argv[2];		//Recipient's name
		message   = argv[3];		//Message

		//Check if message is empty
		if(message[0] == '\0'){
			printf("Invalid message.\n");
			return 0;
		}

		//Send message
		retval = send_msg(recipient, message, curr_user);

		//Check if message sent successfully
		if(retval < 0){
			printf("Error in sending message.\n");
		} else {
			printf("Message sent to %s.\n", recipient);
		}

	//If user wants to get message(s)
	} else if(get_mess == 0){

		//Loop until all messages have been retrieved
		while(stop != 1){

			//Set up strings to hold message and sender
			message = (char*)malloc(sizeof(char)*257);  //Message up to 256 chars
			sender  = (char*)malloc(sizeof(char)*33);   //Username up to 32 chars
			message[0] = '\0';

			//Get message
			retval  = get_msg(curr_user, message, sender);

			//Check return value of get_msg()
			if(retval < 0){		//Error during message retrieval
				printf("Error in retrieving message.\n");
				stop = 1;

			} else {		//No error, print message if there is one

				//Print message or inform of no new messages
				if(message[0] == '\0'){	//If no new message
					printf("No new messages.\n");
				} else {		//Message exists
					printf("%s said: %s\n", sender, message);
				}

				//If no more messages, set stop
				if(retval == 0){
					stop = 1;
				}
			}

			free(message);		//Free allocated memory for message
			free(sender);		//Free allocated memory for sender username
		}

	} else {
		printf("Invalid arguments. Try again.\n");
	}

	return 0;
}
