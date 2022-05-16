/*
* File: osmsg.c
* Author: Kaden Herr
* Date Created: Feb 18, 2022
* Last editted: Feb 18, 2022
* Purpose: A program for users to message each other.
* Compile Note: gcc osmsg.c -o osmsg
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

// Wrapper function for syscall 443
int send_msg(char *to, char *msg, char *from) {
    return syscall(443, to, msg, from);
}

// Wrapper function for syscall 444
int get_msg(char *to, char *msg, char *from) {
    return syscall(444, to, msg, from);
}


int main(int argc, char *argv[]) {
    // Program Variables
    char *user;
    char from[32];
    char msg[256];
    int retVal;

    // Get the current user
    user = getenv("USER");

    // No arguments given.
    if(argc <= 1) {
        fprintf(stderr,"ERROR: No command-line arguments given.\n");
        return 1;
    }

    // See if it is a send message command
    if(argc == 4 && strcmp(argv[1],"-s") == 0) {
        if(send_msg(argv[2],argv[3],user) < 0) {
            fprintf(stderr,"ERROR: An error occured while sending your message.\n");
            return 1;
        }
        
    // See if it is a receive message command
    } else if(argc == 2 && strcmp(argv[1],"-r") == 0) {
        
        retVal = get_msg(user,msg,from);

        if(retVal < 0) {
            printf("You have no messages.\n");
            return 0;
        } else {
            printf("%s said: \"%s\"\n",from,msg);

            // If there are more messages to print
            if(retVal > 0) {
                while(get_msg(user,msg,from) > 0) {
                    printf("%s said: \"%s\"\n",from,msg);
                }
                // Print the last message
                printf("%s said: \"%s\"\n",from,msg);
            }
        }
                
    // It is not a send or receive command, thus it is an error
    } else {
        fprintf(stderr,"ERROR: Incorrect command-line arguments.\n");
        return 1;
    }

    return 0;

}