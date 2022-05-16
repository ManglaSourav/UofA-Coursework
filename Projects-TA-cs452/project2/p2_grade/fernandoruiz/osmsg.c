/*
* File:    osmsg.c
* Author:  Fernando Ruiz 
* Purpose: The following program is an os messager application that uses two created syscalls
* to send and get message across users on the system. 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/syscall.h>
#include <unistd.h>

void parseLine(char *line, char *user_name);
int send_msg(char *to, char *msg, char *from);
int get_msg(char *to, char *msg, char *from);
int checkLine(char *line);

/*
*  main(int argc, char *argv[]) -- The following function calls parseline to conduct the commands
*  for the osmsg application.
*/
int main(int argc, char *argv[]){
    char *line = NULL, *user_name;
    size_t len = 0;

    user_name = getenv("LOGNAME");
    printf("Welcome %s to OS messanger!\n", user_name);
    printf("~ Send Line Format: osmsg -s receiver_name \"message\" \n");
    printf("~ Get  Line Format: osmsg -r \n");
    printf("Type quit to close Application.\n");
    while (getline(&line, &len, stdin) != EOF) {
        if(checkLine(line) == 0){
            char *copyLine = strdup(line);
            char *quitCheck = strtok (copyLine," \n");
            if ((strcmp("quit", quitCheck) == 0)) {
                break;
            }
            parseLine(line, user_name);
        }
    }
    free(line);
}

/*
* checkLine(char *line) -- returns 1 if the line cotians only white space, which indicates a empty line.
*/
int checkLine(char *line){
    int whiteSpace = 0, i = 0;
    for(;*line; line++){
        if (isspace(*line)){
            whiteSpace++;
        }
        i++;
    } 
    
    //if equal empty line
    if(i == whiteSpace){
        return 1;
    }
    return 0;
}

/*
* parseLine(char *line, char *user_name) -- The following function parses the lines
* from STDIN to conduct send and get commands. 
*/
void parseLine(char *line, char *user_name) {
    char *str, *cmd = NULL, *receiver = NULL;
    char msg[160], from[64];
    int msgVal = 1, sendVal = 1, osmsg = 1;

    str = strtok (line," \n");
    while (str != NULL){
        if (osmsg == 1){
            if ((strcmp("osmsg", str) == 0)){
                osmsg = 0;
            } else {
                printf("Line does not fit format. Try again!\n");
                break;
            }
        } else if (osmsg == 0 && cmd == NULL) {
            if ((strcmp("-r", str) == 0)) {
                while(msgVal == 1) {
                   msgVal = get_msg(user_name, msg, from);
                   if (msgVal == 1 || msgVal == 0) {
                       printf("%s said: %s\n", from, msg);
                   }
                   if (msgVal == 0) {
                       printf("No more messages.\n");
                   }
                   if (msgVal == -1) {
                       printf("No messages.\n");
                   }
                }
                break;
            } else if ((strcmp("-s", str) == 0)) {
                cmd = str;
            } else {
                printf("Line does not fit format. Try again!\n");
                break;
            }
        } else if (osmsg == 0 && cmd != NULL) {
            receiver = str;
            str = strtok (NULL, "\n");
            if (str != NULL && str[0] == '"' && str[strlen(str)-1] == '"'){
                sendVal = send_msg(receiver, str, user_name);
                if (sendVal == 0) {
                    printf("Delivered.\n");
                } else {
                    printf("Not Delivered.\n");
                }
                break;
            } else {
                printf("Line does not fit format. Try again!\n");
                break;
            }
        }
        str = strtok (NULL, " \n");
    }
}

/*
* send_msg(char *to, char *msg, char *from) -- The following function returns 0 when a 
* successful message has been to sent to a user from the current user and -1 when the message
* fails to send. Wrapper fcn for syscall.
*/
int send_msg(char *to, char *msg, char *from) {
    int retVal;
    retVal = syscall(443, to, msg, from);
    return retVal;
}

/*
* get_msg(char *to, char *msg, char *from) -- The following function returns the message
* and sender of the receiver/current user. Returns 1 if more messages, 0 if no more, and -1
* if error or never sent a message before. Wrapper fcn for syscall.
*/
int get_msg(char *to, char *msg, char *from) {
    int retVal;
    retVal = syscall(444, to, msg, from);
    return retVal;
}