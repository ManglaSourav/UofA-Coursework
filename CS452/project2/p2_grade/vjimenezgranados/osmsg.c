/*
* File: osmsg.c
* Purpose: Reads in parameters from the command line, and either reads or sends messages to the specified user.
* 
* Author: Victor A. Jimenez Granados
* Date: Feb. 20, 2022
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

int send_msg(char* to, char* msg, char* from);
int get_msg(char* to, char* msg, char* from);

int main(int argc, char* argv[]) {
    //Checks to see if there are at least 1 argument.
    if (argc > 1) {
        //Checks to see if it is a send command.
        if (strcmp("-s", argv[1]) == 0) {
            //If there are 4 parameters, call send_msg syscall.
            if (argc == 4) {
                send_msg(argv[2], argv[3], getenv("USER"));
            }
        }//Checks to see if it is a read command.
        else if (strcmp("-r", argv[1]) == 0) {
            char message[256];
            char fromMessage[256];
            //Reads messages until get_msg returns != 1.
            while (get_msg(getenv("USER"), message, fromMessage) == 1) {
                printf("%s said: \"%s\"\n", fromMessage, message);
            }
        }
    }
    else {
        //Cover's User incorrect input.
        printf("Incorrect arguments, please try again.\n");
    }
    return 0;
}

/*
* Purpose: Calls the send_msg syscall
*/
int send_msg(char* to, char* msg, char* from) {
    long sta = syscall(443, to, msg, from);
    return (int) sta;
}

/*
* Purpose: Calls the read_msg syscall
*/
int get_msg(char* to, char* msg, char* from) {
    long sta = syscall(444, to, msg, from);
    return (int) sta;
}