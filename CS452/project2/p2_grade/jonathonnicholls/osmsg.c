#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>  

int send_msg(char *to, char *msg, char *from){
   return syscall(443, to, msg, from); //returns 0 if it was successful
}

int get_msg(char *to, char *msg, char *from){
    return syscall(444, to, msg, from); //returns 0 if there are no messages
}

int main(int argc, char *argv[]){
    int exitCode; //reflects the return value of the syscalls
    if (argc < 2){
        exit(1);
    }
    if(!strcmp(argv[1], "-r")){ //for reading messages, prints nothing if there are no messages
        char to[64];
        strcpy(to, getenv("USER")); //current user would be recieving messages
        char from[64];
        char msg[1024];
        while((exitCode = get_msg(to, msg, from)) > 0){ //loops until it gets no messages
            printf("%s said: %s\n", from, msg);// prints the message from the sender
        }
    }else if(!strcmp(argv[1], "-s") && argc == 4){ // for sending messages
        char *from = getenv("USER"); //curent user would be sending messages
        exitCode = send_msg(argv[2], argv[3], from);
    }else{ //handles anything else that is not send or receive
        printf("Need correct command line option\n");
        exit(1);
    }
    return exitCode;
}