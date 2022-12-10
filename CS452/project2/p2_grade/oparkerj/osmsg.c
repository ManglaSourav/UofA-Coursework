/*
 * Author: Parker Jones
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// How long of a string can be used for username or msg
#define MAX_LENGTH 1024

// This macro returns a string pointer to the username of the
// current user.
#define USERNAME (getenv("USER"))

/*
 * Wrapper for the send-message syscall.
 */
int send_msg(char *to, char *msg, char *from)
{
    return syscall(443, to, msg, from);
}

/*
 * Wrapper for the get-message syscall.
 */
int get_msg(char *to, char *msg, char *from)
{
    return syscall(444, to, msg, from);
}

/*
 * Send a message from the current user to the specified user.
 */
void send(char *to, char *msg)
{
    // Check that the inputs are within the bounds
    if (strlen(to) > MAX_LENGTH)
    {
        printf("Username longer than max allowed length (%d)\n", MAX_LENGTH);
        return;
    }
    if (strlen(msg) > MAX_LENGTH)
    {
        printf("Message longer than max allowed length (%d)\n", MAX_LENGTH);
        return;
    }

    // Send message and report the results
    int result = send_msg(to, msg, USERNAME);
    if (result)
    {
        printf("There was an error while trying to send the message.\n");
        return;
    }
    printf("Message sent to %s.\n", to);
}

/*
 * Print out all messages sent to the current user.
 */
void receive()
{
    char from[MAX_LENGTH + 1];
    char msg[MAX_LENGTH + 1];
    char *to = USERNAME;

    // Try to read a message for the user
    int result = get_msg(to, msg, from);
    if (result == -1)
    {
        printf("No messages found for the current user.\n");
        return;
    }
    if (result < 0)
    {
        // This return value happens if you pass in a bad pointer,
        // which shouldn't happen.
        printf("There was an error while trying to read messages.\n");
        return;
    }

    // Send all messages
    for (;;)
    {
        printf("%s said: \"%s\"\n", from, msg);
        if (result)
        {
            // If there is another message, fetch it and print again
            result = get_msg(to, msg, from);
            continue;
        }
        break;
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) goto usage;

    // See which action needs to execute
    char *action = argv[1];
    if (strcmp(action, "-s") == 0)
    {
        if (argc < 4)
        {
            printf("Usage: omsg -s <to> <msg>\n");
            return 0;
        }
        // Send a message
        send(argv[2], argv[3]);
    }
    else if (strcmp(action, "-r") == 0)
    {
        // Read messages
        receive();
    }
    else goto usage;
    return 0;

    // Display the usage of the program
    usage:
    printf("Usage: omsg -r|-s [<to> <msg>] \n");
    return 0;
}