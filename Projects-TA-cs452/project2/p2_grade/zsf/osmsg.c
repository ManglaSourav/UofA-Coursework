
/**
 * Author: Zachary Florez 
 * Course: CSC 452 - Spring 2022
 * File: osmsg.c 
 * Project: Project 2 Syscalls 
 * 
 * Description: This file is the userspace application that allows a user to send
 *              and recieve short text messages. To send a message the user will 
 *              issue a command like:
 * 
 *                      osmsg -s jmisurda "Hello World"
 * 
 *              That will queue up a message to be sent to the user jmisurda when 
 *              they get their messages with the following command: 
 *                      osmsg -r 
 * 
 *              For example, if user bob123 sent you "Hello World", output would be:
 *                      bob123 said "Hello World" 
 * 
 *              If there is more than one more message waiting to be recieved, they 
 *              will all be displayed at once. And a message is discarded from the 
 *              system after it has been read. 
 *                  
 */

#include <string.h>
#include <sys/syscall.h> 
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h> 

// Function headers here so we dont get the warning 
int send_message(char *from, char *to, char *message);
int get_message(char *from, char *to, char *message); 

int main(int argc, char *argv[]) {

    // First check to make sure we have the correct num 
    // of args. 
    if (argc != 2 && argc != 4) {
        printf("Did not use 2 or 4 arguments, Exiting now...\n\n");
        return -1; 
    }

    // If argc is 2 we need to need to check to make sure 
    // we have the '-r' flag. 
    if (argc == 2) {
        if (strcmp("-r", argv[1]) != 0) {
            printf("Missing the -r flag, Exiting now...\n\n");
            return -1; 
        }
    }

    // If argc is 4 then we need to check if the "-s" flag 
    // is there. 
    if (argc == 4) {
        if (strcmp("-s", argv[1]) != 0) {
            printf("Missing the -s flag, Exiting now...\n\n");
            return -1; 
        } 
    }

    // When we get here we know that we have a right call 
    // with the correct flags. 
    

    // User getenv() to get the users name and the appropriate 
    // environment variable. 
    char user[20]; 
    strcpy(user, getenv("USER"));

    if (strcmp(argv[1], "-r") == 0) {

        // Getting the message
        char message[256]; 
        char from[20]; 

        // Loop through until there are no more messages 
        while (get_message(from, user, message) == 1) {
            printf("%s sent this: %s\n", from, message); 
        }

        // Sucessful get 
        return 0; 
    } else {
        // Sending the message now. 

        int status = send_message(user, argv[2], argv[3]); 

        if (status != 0) {
            printf("There was a fail that occured with your send :( Please try again \n\n");
            return -1; 
        } else {
            printf("Your send went through! Thank you :)\n");
            return 0; 
        }

    }

    return -1; 
}



/** 
 * Function to sending a message over, just call the system call. 
 */ 
int send_message(char *from, char *to, char *message) {
    return syscall(443, to, message, from); 
}

/**
 * Function to get the message, just call the system call. 
 */
int get_message(char *from, char *to, char *message) {
    return syscall(444, to, message, from); 
}

