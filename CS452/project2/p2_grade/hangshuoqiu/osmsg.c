#include <stdio.h>
#include <string.h>

int send_msg(char *to, char *msg, char *from) {
       return syscall(443, to, msg, from);
}

int get_msg(char *to, char *msg, char *from) {
       return syscall(443, to, msg, from);
}

int main(int argc, char **argv) {
    if (argc != 4 && argc != 2) {
        return -1;
    } else if (argc == 4){
        return send_msg(argv[2], argv[3], getenv());
    } else {
        char msg[1024];
        char from[64];
        int val = get_msg(getenv("USER"), msg, from);

        printf("%s said: \"%s\"", from, msg);

        while (val ==1) {
            memset(msg, 0, sizeof(msg));
            memset(from, 0, sizeof(from));

            val = get_msg(getenv("USER"), msg, from);
            if (val >= 0) {
                printf("%s said: \"%s\"", from, msg);
            }
        }
        return val;
    }

}