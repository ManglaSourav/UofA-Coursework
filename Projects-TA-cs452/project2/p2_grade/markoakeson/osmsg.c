/**
 * Author: Mark Oakeson
 * Course: CSc 452
 * Assignment: Project 2: Syscalls
 * Instructor: Dr. Misurda
 * Due date: 02/20/22
 *
 * The purpose of this program is to allow users to send and receive messages through the linux kernel.
 * The program does this by taking the user input and parsing it to find out if the current user wants
 * to read or send a message. Reading a message takes the form of this on the command line:
 *                                  ./osmsg -r
 * And that will read all messages to the addressed to the current user, and print out in the form of:
 *                              "Sender said: Message"
 * Where sender is the person who sent the message.  This will continue until there are no more unread messages
 * addressed to the current user.  And once a message is read, it is erased.
 *
 * Writing a message takes the form of this on the command line:
 *                                  ./osmsg -s recipient "Message"
 * Where recipient is the user you want to send the message to, and message is the user's message.
 * The message must be in double quotes or the program will only send the first part of the message
 *
 * Messages are capped off at 64 characters, and usernames are capped at 16 characters
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>

/**
 * Wrapper function to make it look more readable in C.  The purpose is to send a
 * message to the user in the kernel space, where it will be stored it a linked list node
 * @param to A char pointer pointer to the recipient
 * @param msg A char pointer pointer to the message
 * @param from A char pointer pointing to the current user/sender
 * @return An int, -1 for and error and 0 for successfully sent message
 */
int send_msg(char *to, char *msg, char *from) {
    return syscall(443, to, msg, from);
}

/**
 * Wrapper function to make it look more readable in C.  The purpose is to retrieve a
 * message to the user in the kernel space, from the kernel Linked List
 * @param to A char pointer pointer to the recipient
 * @param msg A char pointer pointer to the message
 * @param from A char pointer pointing to the current user/sender
 * @return An int, -1 for an error, 0 for no more messages to be read, or 1 for more messages to read
 */
int get_msg(char *to, char *msg, char *from) {
    return syscall(444, to, msg, from);
}


int main(int argc, char* argv[]){
    char to[16];
    char msg[64];
    char from[16];
    char user[16];
    strncpy(user,getenv("USER"), 16);
    int retval = 1;

    if(argc == 1){ // Return -1 if not enough arguments passed
        fprintf(stderr, "ERROR: OSMSG needs a system call of either: '-r' or '-s recipient \"message\"\n");
        return -1;
    }


    if(strncmp(argv[1], "-s", 2) == 0){ // If user wants to send a message
        if(argc < 4){ //Not enough arguments passed in
            fprintf(stderr, "ERROR: Not enough arguments to send message. '-s' recipient \"message\"\n");
            return -1;
        }

        printf("Sending Message...\n");
        strncpy(to, argv[2], 16);
        strncpy(msg, argv[3], 64);
       retval = send_msg(to, msg, user);
       if(retval != 0){
           fprintf(stderr, "ERROR: Could not send message into kernel.\n");
           return -1;
       }
       printf("Message Sent!\n");

    }
    else if(strncmp(argv[1], "-r", 2) == 0){
            printf("Reading Messages...\n");
            retval = get_msg(user, msg, from);
            if(retval == 0){ // User has no unread messages
                printf("No new messages.\n");
            }
            else if(retval < 0){
                fprintf(stderr, "ERROR: Issue retrieving message from kernel.\n");
                return -1;
            }
            else {
                while (retval == 1) { // Loop to retrieve all unread messages for user
                    printf("%s said: %s\n", from, msg);
                    retval = get_msg(user, msg, from);
                }
                printf("End of Messages\n");
            }
    }
    else{
        fprintf(stderr, "Error: Wrong format for entry\n");
        return -1;
    }
}
