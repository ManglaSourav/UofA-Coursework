#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char ** argv)
{
	char command[2];
	char recipient[20];
	char from[20];
	char msg[100];
	char user[20];
	int status;
	size_t msg_len;
	size_t from_len;
	
	strcpy(command,argv[1]);
	strcpy(user,getenv("USER"));

	//if first arg is not an -s (send) or -r (read), then return as its invalid
	
	if((strncmp(command,"-s",2) != 0) && (strncmp(command,"-r",2) != 0))
	{
		printf("invalid command \n");
		return 0;
	}
	
	//if sending message
	if(strncmp(command,"-s",2)==0)
	{
		strcpy(recipient,argv[2]);
		strcpy(msg,argv[3]);
		status = csc452_send_msg(&recipient, &msg, &user);
		if(status == 0)
		{
			printf("Send was successful\n");
		}
		else
		{
			printf("There was an error sending message\n");
		}
	}
	else //checking messages
	{
		printf("reading messages...\n");
		msg_len = 100;
		from_len = 20;
		do
		{
			status = csc452_get_msg(&user, &msg, msg_len, &from, from_len);
			printf("%s said: %s \n", from, msg);
		} while(status == 1);
		if(status < 0)
		{
			printf("There was an error \n");
		}
	}
	return 0;
}
