#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int DEBUG = 0;


int send_msg(char *to, char *msg, char *from) {
    return syscall(443, to, msg, from);
}

int get_msg(char *to, char *msg, char *from) {
    return syscall(444, to, msg, from);
}

int main(int argc, char *argv[]) {
    // The situation that there is no enough argument
    if (argc < 2) {
        printf("There is no enough argument\n");
        return -1;
    }

    char *user = getenv("USER");
    if (DEBUG) { printf("user: %s\n", user); }

    // The situation is to send the message
    if (DEBUG) { printf("argv[1]: %s\n", argv[1]); }
    int rv = 0;
    if (strcmp(argv[1], "-s") == 0 && argc == 4) {
        if (DEBUG) { printf("reached send_msg branch!\n"); }
        if (DEBUG) { printf("to: %s msg: %s  from: %s\n", argv[2], argv[3], user); }

        rv = send_msg(argv[2], argv[3], user);

        if (DEBUG) { printf("ran send_msg with return value: %d\n", rv); }
    }
    // The situation is to receive the message
    else if (strcmp(argv[1], "-r") == 0 && argc == 2) {
        if (DEBUG) { printf("reached get_msg branch!\n"); }

        char *from = malloc(sizeof(char *) * 256);
        char *msg = malloc(sizeof(char *) * 256);
        rv = get_msg(user, msg, from);

        if (DEBUG) { printf("ran get_msg with return value: %d\n", rv); }

        if (rv != -1) {
            printf("%s said: %s\n", from, msg);
        } else {
            printf("There is no message to you or there is an error\n");
            free(from);
            free(msg);
            return -1;
        }

        // Get all messages sent to you
        while (rv) {
            rv = get_msg(user, msg, from);

            if (DEBUG) { printf("ran get_msg with return value: %d\n", rv); }

            if (rv != -1) {
                printf("%s said: %s\n", from, msg);
            }
        }
        free(from);
        free(msg);
    }
    //The situation that the argument format is wrong
    else {
        printf("The argument(s) format is wrong\n");
        return -1;
    }

    if (DEBUG) { printf("reached the end\n"); }
    return 0;
}
