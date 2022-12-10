//
//  osmsg.c
//
//  DanhPham
//  using 443 and 444 syscall to load and read messages to and from the kernel

#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>

int main(int argc, const char * argv[]) {
    // check if there there are an appropriate number of arguments
    if(argc > 2){
        // check option
        const char* option = argv[1];
        // send message
        if(strcmp(option, "-s") == 0){
            syscall(443, argv[2], argv[3], getenv("USER"));
            return 0;
        }
        // read message
        if(strcmp(option, "-r") == 0){
            char *from = calloc(65,sizeof(char)), *msg = calloc(255,sizeof(char));
            syscall(444, getenv("USER"), msg, from);
            printf("%s said: %s", from, msg);
            return 0;
        }
    }
    return 1;
}
