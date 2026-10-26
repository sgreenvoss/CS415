#include <unistd.h>

void writeMsg(char* error_msg, int loc) {
    write(loc, error_msg, strlen(error_msg));
}