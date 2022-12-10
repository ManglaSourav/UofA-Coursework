/* Filename: osmsg.c
 * Author: Gerry Guardiola
 * Purpose: Program helps implement a newly defined syscall to tranfer
 * messages between users in a fashion similar to email. 
 * Known Issues: Receiving messages does not take into account the sender
 * that is managed by the syscall. Syscall implementation not completed. 
 */

#include<linux/kernel.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Takes pointers to strings and uses syscall() function to call upon 
 * csc452_send_msg syscall to send ainput string to another user. 
 * const char __user *to  pointer to string of recipient
 * const char __user *msg pointer to string message
 * const char __user *from pointer to string name of sender
 * returns integer
 */
asmlinkage long sys_csc452_send_msg(const char __user *to,
	       			    const char __user *msg, 
				    const char __user *from) {
	syscall(443, to, msg, from);  // invoke syscall 443
	return 0;
}

/*
 * Takes pointers to strings and uses syscall() function to call upon
 * csc452_get_msg syscall to print out a string sent to this user. 
 * const char __user *to pointer to string of recipient
 * const char __user *msg pointer to string message
 * const char __user *from pointer to string name of sender
 * returns integer depending on status of queued messages (0 = none 1 = more)
 */
asmlinkage long sys_csc452_get_msg(const char __user *to, 
				   const char __user *msg, 
				   const char __user *from) {
	syscall(444, to, msg, from);  // invoke syscall 444
	return 0;  // default to 0
}

int main(int argc, char **argv) {
	// must have command line arguments 
	if (argc <= 1) {
		printf("Missing command line arguments.\n");
		return -1;
	}
	char *arg1 = argv[1];  // this determines which function to call 
	char *user = getenv("USER");  // get user of this current environment 
	if (strcmp(arg1, "-s") == 0) {
		sys_csc452_send_msg(argv[2], argv[3], user);
	}
	if (strcmp(arg1, "-r") == 0) {
		sys_csc452_get_msg(user, argv[2], user);
	}
}
