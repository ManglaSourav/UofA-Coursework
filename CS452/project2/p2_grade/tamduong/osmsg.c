#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include <sys/syscall.h>     
#include <unistd.h>

/**
* send_msg(to, msg, from) will do a syscall to enqueue the message
* to the system
*/
int send_msg(char *to, char *msg, char *from) {
    return syscall(443, to, msg, from);
}
/**
 * get_msg(to, msg, from) will do a syscall to get all the messages
 * directed to the current user of the command line
 */
int get_msg(char *to, char *msg, char *from) {
    return syscall(444, to, msg, from);
}
int main (int argc, char *argv[]) {
    // check for the correct format
    if (argc != 2 && argc!=4) {
        printf("usage: osmsg [-r] | osmsg [-s] [to_user] [msg]\n" );
        return -1; 
    } 
    if (argc == 4) {
        if (strcmp(argv[1], "-s") != 0) {
            printf("usage: osmsg [-r] | osmsg [-s] [to_user] [msg]\n");
            return -1;
        }
    } else {
        if (strcmp(argv[1], "-r") != 0) {
            printf("usage: osmsg [-r] | osmsg [-s] [to_user] [msg]\n");
            return -1;
        }
    }
    //get the user name
    char user[33]; 
    strcpy(user, getenv("USER"));

    // send the message from the user to the other user
    if (strcmp(argv[1], "-s") == 0) {
        if (send_msg(argv[2], argv[3], user) == 0) {
            printf("Send successful\n"); 
            return 0; 
        } else {
            printf("Send failed\n"); 
            return -1;
        }
    } else {
        // get the message on the queue directed to the current user
        char * from = malloc(33); 
        char * msg = malloc(2048);
        while (get_msg(user, msg, from) == 1) {
            printf("%s said: %s\n", from, msg);
        }
        free(from);
        free(msg);
        return 0; 
    }
    return -1; 

    
}