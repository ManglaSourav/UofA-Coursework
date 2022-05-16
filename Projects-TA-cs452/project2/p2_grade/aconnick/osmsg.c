#include <sys/syscall.h>
#include <stdio.h> 
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
/*
 *File: osmsg.c
 *Author: Austin Connick
 *Purpose: uses two new syscalls
 *         to send a message from one user to another
 */


/*
 *send_msg uses syscall sys_csc452_send_msg to add a
 *message to the linked lists
 */
int send_msg(char *to, char *msg, char *from){
      int k; 
      k = syscall(443,to,msg,from);
   return k;
} 
/*
 *get_msg uses syscall sys_csc452_get_msg to pull
 *a message from the linked list returning zero for end of
 *messages and one if more message can be read
 */
int get_msg(char *to, char *msg, char *from){
      int i; 
      i = syscall(444,to,msg,from);     
   return i;
}  

/*
 *main reads in from the command line and will send messages
 *or read all the messages for the user
 */
int main(int argc, char *argv[]){
     
     char *username;
     char msgBuff[256];
     char fromBuff[256]; 
   
     if(argc == 4  && strcmp(argv[1],"-s") == 0){
         username = getenv("USER"); 
         send_msg(argv[2],argv[3],username);
     }else if(argc == 2 && strcmp(argv[1], "-r") == 0){
         username = getenv("USER");
         while(get_msg(username,msgBuff,fromBuff)){
            printf("%s said: %s\n",fromBuff,msgBuff);

         } 
     }else{
         fprintf(stderr,"invalid args: to read osmsg -r : to send: msg -s to msg\n"); 
    } 

    return 0;
}
