/**This file will use the syscalls that we defined
 * */
/**grab the syscalls*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#define __NR_csc_452_send_msg 443
#define __NR_csc_452_get_msg 444

int send_msg(char *to, char *msg, char* from){
	return syscall(443, to, msg, from);
	
}

int get_msg(char *to, char *msg, char* from){
	return syscall(444, to, msg, from);
	
}
void build_msg(){
	int retval = 0;
	//	to	msg	from
	retval = send_msg("user1", "hola", "root");
	printf("msg1: %d\n", retval);
	retval = send_msg("user1", "hola", "hassan");
	printf("msg2: %d\n", retval);
	retval = send_msg("user1", "hola", "user4");
	printf("msg3: %d\n", retval);
	retval = send_msg("user1", "hola", "user3");
	printf("msg4: %d\n", retval);
	retval = send_msg("user1", "hola", "user2");
	printf("msg5: %d\n", retval);


}
void get_msgs(char *user){
	char msg[100];
	char from[100];
	char *to = user;
	int check = (get_msg(to, msg, from));
	while (check >= -3){
		if(check == -2){
			printf("kernel err: could not copy all bytes\n");
			break;
		}
		if(check == -2){
			printf("No messages for %s\n", user);
			break;
		}
		else if(check == -1){
			printf("No messages on stack\n");
			break;
		}else if (check == 0){
			printf(" from: %s\n msg: %s\n", from, msg);
			printf("No more messages\n");
			break;
		}else{
			printf(" from: %s\n msg: %s\n", from, msg);
			check = (get_msg(to, msg, from));
		}
		printf("---------------\n");
	}
}
int main(int argc, char * argv[]){
	int retval;
	char *user, *receiver, *msg;
	//get the arguments
	if (argc >= 5) {printf("Err: args are more than expected\n") ; return 1;}
	if (argc < 2) {printf("Err: args are less than expected\n") ; return 1;}
	user = getenv("USER");
	if(!strcmp(argv[1], "-r")){
		get_msgs(user);

	}else if(!strcmp(argv[1], "-s")){
		if (argc != 4) {printf("Err: args are more than expected\n") ; return 1;}

		
		receiver = argv[2];
		msg = argv[3];

		//	to	msg	from
		retval = send_msg(receiver, msg, user);
		if(retval == -1) {printf("Err: send_msg kernel err\n") ; return 1;}

	}
	return 0;


}
