/**
 * @file osmsg.c
 * @author Daniel Ryngler
 * @brief csc 452 Hw 2
 * @version 0.1
 * @date 2022-02-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNUSED(x) (void)(x)


/**
 * @brief Send a message for a user using the sys call implemented in sys.c
 * 
 * @param to, to send to
 * @param msg, message
 * @param from, sending from
 * @return int -> 0 (msg sent succesfully), -1 (error) 
 */
int sendMsg(char *to, char *msg, char *from) {
    return syscall(443, to, msg, from);
}

/**
 * @brief Receive a message for a user using the sys call implemented in sys.c
 * 
 * @param to, sent to
 * @param msg, message
 * @param from, sent from
 * @return int. -1 (no msg found), 0 (single msg found), 1 (mutliple msg found)
 */
int getMsg(char *to, char *msg, char *from) {
    return syscall(444, to, msg, from);
}

/**
 * @brief Demo program showing both sys calls work appropriately
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char **argv) {

    char *command;
    char *toUser;
    char *message;
    char *currUser;

    // read command line args
    if (argc > 1) {
        UNUSED(*argv++);
        command = *argv++;
        if (strcmp(command, "-s") == 0) {
            if (argc != 4) {
                fprintf(stderr, "You need to pass a username and message with the send command\n");
                exit(1);
            }
            toUser = *argv++;
            message = *argv++;
        } else if (strcmp(command, "-r") != 0) {
            fprintf(stderr, "Invalid argument passed: %s\n", command);
            exit(1);
        }
        currUser = getenv("USER");
    } else {
        fprintf(stderr, "No arguments passed \n");
        exit(1);
    }

    // send message
    if (strcmp(command, "-s") == 0) {
        int retVal = sendMsg(toUser, message, currUser);
        if (retVal != 0) {
            fprintf(stderr, "Error sending messeage. Are you in the correct kernel? \n");
            exit(1);
        }
        printf("Message sent successfully\n");
    } 

    // read message
    else if (strcmp(command, "-r") == 0) {
        char msgReceived[250]; 
        char fromUser[250];
        int retVal = getMsg(currUser, msgReceived, fromUser);
        if (retVal == -1) {
            printf("%s, you have no messages\n", currUser);
        } else {
            printf("%s said: %s \n", fromUser, msgReceived);

            if (retVal == 0) {
            printf("%s, this was your only message\n", currUser);
            } else if (retVal == 1) {
                printf("%s, you have more messeages waiting for you\n", currUser);
            }
        }
    }

    return 0;
}
