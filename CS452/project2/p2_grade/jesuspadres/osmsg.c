#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
/**
  Author: Jesus Padres
  Class: Csc 452

  Purpose: This program uses system calls to send messages
  to linux users, as well as recieving those messages.

  For example: when you type the command  "osmsg -s jesus hello there";
  and if you are logged in as jesus when you type the command
  "osmsg -r" it outputs "(user) said: hello there"
*/

char *me;       // name of current user
char msg[256];  // message reader storage
char from[64];  // sender reader storage

/**
 * Method read_msg
 *
 * Purpose: this method uses syscall 444 to read and print out any
 * messages that have been sent to *me through osmsg.
 *
 * Pre-condition: syscalls are implemented correctly, me has the
 * appropriate value, and msg and from are ready to recieve a string.
 *
 * Post-condition: terminal prints the appropriate messages.
 *
 * @return void
 */
void read_msg() {
  int x = syscall(444, me, msg, from);

  while(x >= 0) {
    printf("%s said: %s\n", from, msg);
    x = syscall(444, me, msg, from);
  }
}

/**
 * Method write_msg
 *
 * Purpose: this method uses syscall 443 to send a message to
 * a desired recipient.
 *
 * Pre-condition: syscalls are implemented correctly, me has the
 * appropriate value as well as the input strings.
 *
 * Post-condition: linkedlist in sys.c contains this message.
 *
 * @return int, returns -1 if there was an error sending the message.
 */
int write_msg(char *w_to, char *w_msg) {
  return syscall(443, w_to, w_msg, me);
}


int main(int argc, char *argv[]) {
  me = getenv("HOME")+1;
  int wasError = 0;

  if (argc == 2) {
    if (strcmp(argv[1], "-r") == 0) {
      read_msg();
    }
  } else if (argc >= 4) {
    if (strcmp(argv[1], "-s") == 0) {
      char *new_to = argv[2];
      char new_msg[256] = "";

      int i = 3;
      while (i < argc) {
        strcat(new_msg, argv[i]);
        strcat(new_msg, " ");
        i = i + 1;
      }

      wasError = write_msg(new_to, new_msg);
    }
  }
  return wasError;
}
