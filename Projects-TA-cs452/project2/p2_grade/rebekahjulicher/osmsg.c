#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/*
* File: omsg.c
* Author: Rebekah Julicher
* Purpose: Uses custom syscalls for in-system messaging
*/

// Wrapper function for send_msg syscall
int send_msg(char *to, char *msg, char *from){
	return syscall(443, to, msg, from);
}

// Wrapper function for get_msg syscall
int get_msg(char *to, char *msg, char *from){
	return syscall(444, to, msg, from);
}

// processInput - Handles all input argument processing
void processInput(int argc, char *argv[]){
    char name[65];
    name[0] = '\0';
    char msg[1001];
    msg[0] = '\0';
	char* currUser;
	currUser = getenv("USER");

    if (currUser != NULL){
        // If there are enough args and command is correct, receives messages for current user
        if (argc == 2 && strcmp(argv[1],"-r") == 0){
	    int val;
            val = get_msg(currUser, msg, name);
            while (val > 0){
                printf("%s said: \"%s\"\n", name, msg);
                name[0] = '\0';
                msg[0] = '\0';
                val = get_msg(currUser, msg, name);
            }
            if (name[0] != '\0' || msg[0] != '\0') printf("%s said: \"%s\"\n", name, msg);
        }

        // If there are enough args and command is correct, sends a message for current user
	else if (argc == 4 && strcmp(argv[1],"-s") == 0){
	    strcpy(msg, argv[3]);
            int val;
            val = send_msg(argv[2], msg, currUser);
	    if (val != 0) exit(val);
        }

        else exit(1);
    }
    else exit(1);
}

int main ( int argc, char *argv[] ){
    processInput(argc, argv);
    return 0;
}
