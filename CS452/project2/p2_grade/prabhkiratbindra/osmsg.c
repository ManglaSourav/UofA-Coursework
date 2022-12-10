#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int send_msg(char* to, char* msg, char* from) {
    return syscall(443, to, msg, from);
}

int get_msg(char* to, char* msg, char* from) {
    return syscall(444, to, msg, from);
}

int main(int argc, char* argv[]) {

    char* user = strdup(getenv("USER"));

    char other[101], message[101];

    switch (argc) {

        case 2:     // 1 argument

            if (strcmp(argv[1], "-r") == 0) {

                int retVal;

                do {

                    retVal = get_msg(user, message, other);
                    printf("%s said: %s\n", other, message);

                } while (retVal == 1);

                if (retVal == -1) {
                    fprintf(stderr, "Error getting message.\n");
                }

            } else {
                fprintf(stderr, "Error: wrong usage.\n");
            }

            break;

        case 4:     // 3 arguments

            if (strcmp(argv[1], "-s") == 0) {

                strcpy(other, argv[2]);         // to
                strcpy(message, argv[3]);       // msg

                int retVal = send_msg(other, message, user);

                if (retVal != 0) {
                    fprintf(stderr, "Error sending message.\n");
                }

            } else {
                fprintf(stderr, "Error: wrong usage.\n");
            }

            break;

        default:
            fprintf(stderr, "Error: wrong usage.\n");
            break;
    }

    free(user);
}
