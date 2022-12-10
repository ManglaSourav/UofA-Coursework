#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#define USERNAME_LEN 64
#define MSG_LEN 1024

// wrapper function for csc452_send_msg syscall
int sendMsg(char *to, char *msg, char *from){
    return syscall(443, to, msg, from);
}

// wrapper function for csc452_get_msg syscall
// returns 1 if more msgs to get
// returns 0 if no more msgs to get
// returns -1 if error 
int getMsg(char *to, char *msg, char *from){
    return syscall(444, to, msg, from);
}

// main program
int main(int argc, char* argv[]) {
    printf("\n");

    int result = -1;
    
    char* currentUser = getenv("USER");
    // printf("Current user: %s\n\n", currentUser);
    
    if (argc == 2 && !strcmp(argv[1], "-r")) {
        // Get msgs
        // printf("Getting msgs\n\n");
        
        char *to = currentUser;
        char *msg = malloc(sizeof(MSG_LEN));
        char *from = malloc(sizeof(USERNAME_LEN));
        result = 1;

        // keep getting msgs until no more msgs or error
        while (result == 1) {
            result = getMsg(to, msg, from);
            if (result != -1) {
                printf("%s: %s\n", from, msg);
            }
        }

        if (result<1) {
            printf("No more messages remaining.\n");
        }

    } else if (argc == 4 && !strcmp(argv[1], "-s")) {
        // Send msgs
        char *to = argv[2];
        char *msg = argv[3];
        char* from = currentUser;
        
        printf("Sending the following msg to %s: %s\n", to, msg);

        result = sendMsg(to, msg, from);

    } else {
        printf("Incorrect cmd line args.\n");
        printf("To get msgs: osmg -r\n");
        printf("To send msgs: osmsg -s [target_user] \"[Your msg here]\"\n");
    }

    printf("\n");
    return result;
}