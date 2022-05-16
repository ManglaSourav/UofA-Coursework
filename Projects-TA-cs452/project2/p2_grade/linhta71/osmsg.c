#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <sys/syscall.h>     
#include <unistd.h>

/**
* Author: Linh Ta
* send_msg(to, msg, from) will do a syscall to enqueue the message
* to the system
*/
int send_msg(char *to, char *msg, char *from) {
    return syscall(443, to, msg, from);
}
/**
 * Author: Linh Ta
 * get_msg(to, msg, from) will do a syscall to get all the messages
 * directed to the current user of the command line
 */
int get_msg(char *to, char *msg, char *from) {
    return syscall(444, to, msg, from);
}

int main (int argc, char *argv[]) {
    char user[33]; 
    strcpy(user, getenv("USER"));
    if (argc == 4 && strcmp(argv[1], "-s") == 0) {
        if (send_msg(argv[2], argv[3], user) == 0) {
            printf("Success!!!\n"); 
            return 0; 
        } else {
            printf("Failed\n"); 
            return -1;
        }
    } else if (argc == 2 && strcmp(argv[1], "-r") == 0) {
        //get all the message
        char from[33]; 
        char msg[1000];
        while (get_msg(user, msg, from) == 1) {
            printf("%s said: %s\n", from, msg);
        }
        return 0; 
    }

    printf("osmsg -s [user] [msg] OR osmsg -r\n");
    return -1;
}