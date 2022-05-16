/* File: osmsg.c
   Author: Cole Blakley
   Description: This implements a user messaging system. Users can send a message
     to a user with: ./osmsg -s recipient "message". Messages will be queued.
     Any unread messages for the current user can be read using: ./osmsg -r.
     After a message is read, it is removed from the queue.
*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Send a msg to a given user from a given user. Returns 0 on success,
// -1 on error.
int send_msg(const char* to, const char* msg, const char* from)
{
    return syscall(443, to, msg, from);
}

// Receive a msg that was sent to a given user. An unread message (if found) is
// written out to msg, and its sender is written out to from. Returns 1 if a
// message was written out to msg and the sender was written out to from,
// 0 if there are no more messages, and -1 if an error occurred. If the
// status is 1, then there may be more messages left to receive.
int get_msg(const char* to, char* msg, char* from)
{
    return syscall(444, to, msg, from);
}

static void print_usage()
{
    fprintf(stderr, "Usage: osmsg -r\n       osmsg -s recipient \"message\"\n");
}

int main(int argc, char** argv)
{
    if(argc < 2) {
        print_usage();
        return 1;
    }

    const char* action = argv[1];
    if(strcmp(action, "-r") == 0 && argc == 2) {
        char msg[1024] = {};
        char from[128] = {};
        const char* to = getenv("USER");
        int status = get_msg(to, msg, from);
        if(status == 0) {
            printf("No messages\n");
            return 0;
        } else if(status < 0) {
           fprintf(stderr, "Error: Failed to check for messages\n");
           return 1;
        }
        do {
            printf(" From: %s\n    %s\n", from, msg);
        } while((status = get_msg(to, msg, from)) > 0);
        if(status < 0) {
            fprintf(stderr, "Error: Failed to check for messages\n");
            return 1;
        }
    } else if(strcmp(action, "-s") == 0 && argc == 4) {
        const char* to = argv[2];
        const char* from = getenv("USER");
        const char* msg = argv[3];
        int status = send_msg(to, msg, from);
        if(status == 0) {
            printf("Sent message to %s\n", to);
        } else {
            fprintf(stderr, "Failed to send message to %s\n", to);
            return 1;
        }
    } else {
        print_usage();
    }

    return 0;
}
