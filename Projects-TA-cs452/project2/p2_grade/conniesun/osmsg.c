#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

/*
 * osmsg.c
 *
 * Author: Connie Sun
 * Course: CSC 452 Spring 2022
 * 
 * Userspace application for sending and receiving short messages
 * using system calls. This program accepts two commands:
 *      osmsg -r
 *      osmsg -s recipient "message"
 * The '-r' flag is used to retrieve all messages that have been sent
 * to the current user and print them to the screen. The '-s' flag is
 * used to send a message to another user. Messages are removed from
 * the system once retrieved.
 *
 */

/*
 * Send a message to another user by calling syscall 443,
 * which corresponds to the csc452_send_msg syscall. If 
 * the call fails, print an error message.
 * 
 * to: char *, current user sending the message
 * msg: char *, message to be sent
 * from: char *, intended recipient
 */
int send_msg(char *to, char *msg, char *from) {
    int ret = syscall(443, to, msg, from);
    if (ret < 0)
        fprintf(stderr, "failed to send message\n");
}

/*
 * Retrieve all messages that have been sent to current 
 * user by calling syscall 444, which corresponds to the 
 * csc452_get_msg syscall. The first time this is called,
 * ret of 0 may indicate either 1 message or no messages, 
 * so check the length of from (which should be > 0 if there
 * is a message). Then repeatedly get messages while there
 * are still messages to be retrieved (ret is 1). All 
 * messages are printed to the screen, and an error message
 * is printed if the syscall returns a negative number.
 * 
 * to: char *, current user retrieving messages
 * msg: char *, allocated buffer where message will be returned
 * from: char *, allocated buffer where sender will be returned
 */
int get_msg(char *to, char *msg, char *from) {
    int ret = syscall(444, to, msg, from);
    if (ret < 0)
        fprintf(stderr, "failed to retrieve messages\n");
    else if (strlen(from) > 0) // there is a message to get
    	printf("%s said: \"%s\"\n", from, msg);
    // more messages to get if ret > 0
    while (ret > 0) {
	    ret = syscall(444, to, msg, from);
        if (ret < 0)
            fprintf(stderr, "failed to retrieve messages\n");
	    else
            printf("%s said: \"%s\"\n", from, msg);
    }
}

int main(int argc, char** argv){
    // get the user's name from the environment variable
    char *user = getenv("USER");
    // for retrieving messages: osmsg -r
    if (argc == 2) {
        if (strcmp(argv[1], "-r") == 0) {
            char msg_buf[140]; // max 140 char message
            char from_buf[50]; // max 50 char username
            // make length of from = 0 for checking
	        from_buf[0] = '\0';
            get_msg(user, msg_buf, from_buf);
        }
    }
    // for sending a message: osmsg -s to "message"
    else if (argc == 4) {
        if (strcmp(argv[1], "-s") == 0) {
            char *to = argv[2];
            char *message = argv[3];
            send_msg(to, message, user);
        }
    }
    return 0;
}
