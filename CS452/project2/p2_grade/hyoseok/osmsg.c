/*
** AUTHOR: Hyoseo Kwag
** DATE: CSC452 Project2
** DESCRIPTION: Program that sends and receives messages between users.
*/

#include <linux/kernel.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <stdlib.h>

#include <linux/syscalls.h>
#include <linux/linkage.h>

//#include <linux/init.h>
//#include <linux/sched.h>

// asmlinkage for send_msg syscall
asmlinkage long sys_csc452_send_msg(const char __user *to, const char __user *msg, const char __user *from) {
	return 0;
}

// asmlinkage for get_msg syscall
asmlinkage long sys_csc452_get_msg(const char __user *to, char __user *msg, char __user *from) {
	return 0;
}

// wrapper function for send_msg
int send_msg(char *to, char *msg, char *from) {
	syscall(443, to, msg, from);
	return 0;
}

// wrapper function for get_msg
int get_msg(char *to, char *msg, char *from) {
	syscall(444, to, msg, from);
	return 0;
}

// receive command and call functions depending on parameters
int main(int argc, char **argv) {
	if (argc == 5) {
		// send_msg command; osmsg -s name "msg"
//		printf("%s %s %s %s\n", argv[1], argv[2], argv[3], argv[4]);
		if (strcmp(argv[1],"osmsg")==0 && strcmp(argv[2],"-s")==0) {
//			printf("correct send command\n");
			int result = send_msg(argv[3],argv[4],getenv("LOGNAME"));
		}
	}
	else if (argc == 3) {
		// get_msg command; osmsg -r
//		printf("%s %s\n", argv[1], argv[2]);
		if (strcmp(argv[1],"osmsg")==0 && strcmp(argv[2],"-r")==0) {
//			printf("correct get command\n");

			char *msg = (char*)malloc(100);
			char *from = (char*)malloc(100);
			int result = get_msg(getenv("LOGNAME"), msg, from);
//			printf("%s said: %s\n", from, msg);
		}
	}
	return 0;
}
