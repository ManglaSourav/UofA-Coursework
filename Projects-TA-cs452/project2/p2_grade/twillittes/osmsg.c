/*
 * Author: Taylor Willittes
 * File: osmsg.c
 * Purpose: sending and reading messages from users
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/*
 * calls send mesage with correct syscall number
 */
int send_msg(char *to, char *msg, char *from){
return syscall(443, to, msg, from);
}

/*
 * calls get msg w correct syscall num
 */
int get_msg(char *to, char *msg, char *from){
return syscall(444, to, msg, from);
}

int main(int argc, char **argv){
if (argc >= 2){
	if (strcmp(argv[1], "-r") == 0){ //user gives read command
	long int ret = 1;
	while (ret == 1){
	char msg[50];
	char from[50];
	ret = get_msg(getenv("USER"), msg, from);
	if (msg[0] == 0){
		return 0; //success
	}
	//printf("%d ret: \n", ret);
	printf("%s said: \"%s\"\n", from, msg);
	}
	if (ret < 0){
	printf("Oh noo...\n");  //something went wrong
	return -1; //exittt   status
	}
	} else if (argc >= 4 && strcmp(argv[1], "-s") == 0){ //user uses send cmd
	long int s = send_msg(argv[2], argv[3], getenv("USER"));
	if(s < 0){
	printf("Oh no...\n"); //something wen twrong
	return -1;
	}
	printf("%1d\n", s);
	}else{
	printf("Wrong\n"); //wrong format for send
	}
}else{
printf("Wwwrong\n"); //wrong format for read
}
return 0; //ret at end of code
}



