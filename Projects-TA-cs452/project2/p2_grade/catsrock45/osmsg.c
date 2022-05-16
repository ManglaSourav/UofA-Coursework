#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h> 
#include <unistd.h>
#include <string.h>


/*
 * File : osmsg.c
 * Author : Amin Sennour
 * Purpose : implment a simple messaging program utilizing the system calls 
 *           csc_452_send_msg and csc452_get_msg. 
 */


// define the sizes of the fields of the linked list
#define TO_SIZE 64
#define FROM_SIZE 64
#define MSG_SIZE 1024


// define the cli flags
const char* SEND_MODE = "-s";
const char* READ_MODE = "-r";


/**
 * @brief perform the csc452_send_msg system call to send a message from one 
 *        user to another
 * 
 * @param to the recieving user
 * @param from the sending user
 * @param msg the message
 * @return int 
 */
int send_msg(const char *to, const char *from, const char *msg) {
    return syscall(443, to, msg, from);
}


/**
 * @brief Perform the csc452_get_msg system call to get a single mesage 
 *        for a specified user
 * 
 * @param to the specified user
 * @param from pointer to return the message sender
 * @param msg pointer to return the message
 * @return int 0 if no more messages, 1 if more messages, -1 otherwise
 */
int get_msg(const char *to, char *from, char *msg) {
    return syscall(444, to, msg, from);
}


/**
 * @brief Get the all msgs for a given user
 * 
 * @param to the given user
 * @return int 0 if proper execution, -1 otherwise
 */
int get_all_msgs(const char *to) {
    char from[FROM_SIZE];
    char msg[MSG_SIZE];

    int result;
    do {
        result = get_msg(to, from, msg);
        if (result == 1) {
            printf("%s said : \"%s\"\n", from, msg);
        }

    } while (result != 0);

    return result;
}


/**
 * @brief the main method of osmsg, decided which operation the user is 
 *        requesting (send or read) and performs it.
 * 
 * @param argc the number of arguments 
 * @param argv the list of arguments
 * @return int 0 if proper execution, -1 otherwise
 */
int main(int argc, char const *argv[])
{
    int err;

    // ensure that the user provided the mode flag 
    if (!(argc > 1)) {
        printf("Usage : osmsg -<s|r> \n");
        exit(1);
    }

    // get the current user 
    const char* user = getenv("USER");

    // get the mode flag 
    const char* mode = argv[1];

    // if the mode flag is READ_MODE then print all of the user's messages
    // if the mode flag is SEND_MODE then validate and send the specified 
    // message
    if (strcmp(mode, READ_MODE) == 0) {
        err = get_all_msgs(user);

    } else if (strcmp(mode, SEND_MODE) == 0) {
        // ensure that the user provided the user and message 
        if (!(argc > 3)) {
            printf("Usage : osmsg -s <user> \"<message>\" \n");
            exit(1);
        }
        const char* to = argv[2];
        const char* message = argv[3];

        err = send_msg(to, user, message);
    }

    return err;
}