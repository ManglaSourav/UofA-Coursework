#include <stdio.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MSG_LEN 100


// encapsulating the syscall to normal function
void send_msg(char *to ,char *msg, char *from){
    syscall(443,to,msg,from);
}

// encapsulating the syscall to normal function
void get_msg(char *to,char *msg,char *from){
    // call syscall until no message left 
    int result = syscall(444,to,msg,from);
    while(1){
        printf("%s said: %s\n",from ,msg);
	if(result == 0)
	   break;

	result = syscall(444,to,msg,from);
    }
}

int main(int argc,char *argv[]){
    // check if the command  is used correctly 
    if(argc == 2 && !strcmp("-r",argv[1])){
        char msg[MSG_LEN];
        char usr[MSG_LEN];
        get_msg(getenv("USER"),msg,usr);
    }else if (argc == 4 && !strcmp("-s",argv[1])){
        send_msg(argv[2],argv[3],getenv("USER"));
    }else{ // print help USAGE
        printf("USAGE: \n Sender USAGE: ./osmsg -s (receiverName) (msg)\n Receiver USAGE: ./osmsg -r \n");
    }
    return 0;
}
