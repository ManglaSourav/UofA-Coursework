#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
/**
 *
 * @Author: Philip Jochems
 * @Description: Reasds in a command and either sends a message to another user or retrieves the users messages
 * based on the command.
 *
 */


/**
 * Calls the syscall send message
 * @param to - user that recieves message
 * @param msg - message
 * @param from - user that sends message
 */
int send_msg(char *to, char *msg, char *from){
	return syscall(443,to,msg,from);
}

/**
 * Calls the syscall that checks and retrieves a message if there is one for to.
 * @param to - user that revieves message
 * @param msg - message 
 * @param from - user that sent message
 */
int get_msg(char *to, char *msg, char *from){
	return syscall(444,to,msg,from);
}

/**
 * Reads in a command and executes either get_message or send_message accordingly
 * @param argc - the size of argv
 * @param argv - char array with parameters
 */
int main(int argc, char **argv){
	//No parameters
	if(argc!=2 && argc!=4){
		return -1;
	}

	
	char *command =argv[1];
	char *curr_user = getenv("LOGNAME");
	
	if(strcmp(command,"-s")==0 && argc==4){//set
	char *user = argv[2];
	char *message=argv[3];
	send_msg(curr_user,message,user);
	
	}else if(strcmp(command,"-r")==0 && argc==2){//get
		int getMoreMessages=1;
		while(getMoreMessages==1){
			char msg[252];
			char from[252];
			getMoreMessages=get_msg(curr_user,msg,from);
			if(getMoreMessages==0 || getMoreMessages==1){
				printf("%s said: %s\n",from,msg);
			}
		}
	}else{
		return -1;
	}
	
	return 0;
}
