/*
 * FILE: osmsg.c
 * AUTHOR: Domenic Telles
 * COURSE: CSC 452
 * DESCRIPTION:
 *      This program allows the user to pass small text messages from one user to another by
 *      the way of the kernel.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// SEND_MSG SYSCALL WRAPPER CODE
int send_msg(char* to, char* msg, char* from) {
    return syscall(443,to,msg,from);
}

// GET_MSG SYSCALL WRAPPER CODE
int get_msg(char* to, char* msg, char* from) {
    return syscall(444,to,msg,from);
}

// ARG1: FLAG, ARG2 TOUSER, ARG3 MSG
int main(int argc, char* argv[]) {
    char flag[3];
    char toUser[128];
    char fromUser[128];
    char msg[512];
    int retVal;
    strcpy(flag,argv[1]);
    strcpy(fromUser,getenv("USER"));

    // SEND A MESSAGE
    if (strcmp(flag,"-s") == 0) {
        printf("Sending Message...\n");
        strcpy(toUser,argv[2]);
        strcpy(msg,argv[3]);
        retVal = send_msg(toUser,msg,fromUser); // SEND_MSG SYSCALL
        // MESSAGE SUCCESSFULLY SENT
        if (retVal == 0)
            printf("Message Sent\n");
        // MESSAGE WAS NOT ABLE TO BE SENT
        else {
            printf("ERROR: Unable to Send Message\n");
            return 1;
        }
    }
    // READING MESSAGES
    else if (strcmp(flag,"-r") == 0) {
        printf("Loading Messages...\n");
        // READ ALL MESSAGES
        do {
            retVal = get_msg(toUser,msg,fromUser); // GET_MSG SYSCALL
            printf("%s said: \"%s\"\n",fromPerson,msg);
        } while (retVal == 1);
        // IF MESSAGES ARE UNABLE TO BE READ
        if (retVal < 0) {
            printf("ERROR: Unable to Load Messages\n");
            return 1;
        }
    }
    // INVALID COMMAND
    else {
        printf("ERROR: Invalid Command Flag\n");
        return 1;
    }
    return 0;
}