/* File: osmsg.c
* Author: Flynn Gaur
* Course: CSC 452, Misurda
* Desription: This file verfies the created syscalls 
*             using the syscall function
*/

#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

// functions for syscall
int send_msg(char *to, char *msg, char *from) {
    syscall(443, *to, *msg, *from);
}

int get_msg(char *to, char *msg, char *from) {
    syscall(444, *to, *msg, *from);
}

int main(int argc, char ** argv) {
    char progArg[2];
    char from[50];
    char to[50];
    char msg[100];
    char user[50];
    int retVal = 1;

    // copy program argument to progArg
    strcpy(progArg,argv[1]);
    strcpy(user,getenv("USER"));

    
    // verify invalid prog arg
    if((strncmp(progArg,"-r",2) !=0 ) && (strncmp(progArg,"-s",2) !=0 )) {
        printf("Inavlid arguments entered!\n");
        return -1;
    }

    // verify prog arg read
    if (strncmp(progArg,"-r",2) == 0) {
        printf("Reading Messages!\n");

        // read message once
        
        while(retVal == 1) {
            retVal = get_msg(user, msg, from);
            if(retVal < 0) {
                printf("Error reading message!\n");
                return -1;
            }
            else 
                printf("%s said: %s\n", from, msg);
        }
    }
    
    // verify prog arg send
    else {
        strcpy(to, argv[2]);
        strcpy(msg, argv[3]);
        retVal = send_msg(to, msg, user);
        if (retVal < 0) {
            printf("Error sending message!\n");
            return -1;
        }
        else
            printf("Message Sent!"); 
    }
    return 0;
}
