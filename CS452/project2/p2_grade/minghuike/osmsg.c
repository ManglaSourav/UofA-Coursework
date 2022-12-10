/*
 *  Author:     Minghui Ke
 *  Assignment: CSC452 Project 2
 * 
 *  purpose:    The file will run on the linux and used with two additional syscall made
 *              for store the message in the kernel.
 */ 

#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *  The function call the syscall to send message.
 */ 
int send_msg(char *to, char *msg, char *from) {
    return syscall(443, to, msg, from);
}

/*
 *  The function call the syscall to get message.
 */ 
int get_msg(char *to, char *msg, char *from) {
    return syscall(444, to, msg, from);
}

/*
 *  Use the argumentation to decide what to do.
 */ 
int main( int argc, char *argv[] ) {
    // the function only support for send and get message.
    if (argc != 2 && argc != 4) {
        printf("Please use correct argument!\n");
        return -1;
    }

    char to[32] = "";
    char msg[1024] = "";
    char from[32] = "";

    if (argc == 4) {
        // send message
        int state;
        if (strcmp(argv[1], "-s") == 0) {

            strcpy(to, argv[2]);
            strcpy(msg, argv[3]);
            strcpy(from, getenv("USER"));
            
            state = send_msg(to, msg, from);

            if (state == 0) {
                printf("Message sent!\n");
                return 0;
            } 
            if (state == -1) {
                printf("send failed.\n");
                return -1;
            }
        } else {
            printf("Please use correct argument!\n");
            return -1;
        }
    } else {
        // get message
        if (strcmp(argv[1], "-r") == 0) {

            strcpy(to, getenv("USER"));
            int state;

            while (1) {

                state = get_msg(to, msg, from);
                
                if (msg[0] == '\0') {
                    printf("No message!\n"); 
                    return 0;
                }
                if (state == -1) {
                    printf("Get message failed.\n");
                    return -1;
                }
                printf("%s said: \"%s\"\n", from, msg);

                if (state == 0) return 0;
            }
        } else {
            printf("Please use correct argument!\n");
            return -1;
        }
    }
}