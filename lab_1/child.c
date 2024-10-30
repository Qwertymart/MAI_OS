#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "string.h"

enum status{
    SUCCESS,
    INPUT_ERROR,
    OVERFLOW
};

int is_valid_int(const char* str) {
    if (*str == '\0') return 0;

    for (const char* p = str; *p; p++) {
        if (!isdigit(*p) && *p != '-' && *p != '+') {
            return 0;
        }
    }
    return 1;
}

enum status str_to_int(const char *str, int * result)
{
    if (!(is_valid_int(str))){
        return INPUT_ERROR;
    }
    size_t len = strlen(str);
    if (len > 11 || (len == 11 && str[0] == '-' && str[1] > '2') || (len == 10 && str[0] != '-')) {
        return OVERFLOW;
    }
    char *endptr;
    int temp;
    temp = strtol(str, &endptr, 10);

    if (*endptr != '\0') {
        return INPUT_ERROR;
    }
    *result = (int)temp;
    return SUCCESS;
}

void write_sum(int sum) {
    char msg[256];
    int len = snprintf(msg, sizeof(msg), "Sum: %d\n", sum);
    write(STDOUT_FILENO, msg, len);
}

int main() {
    char line[256];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        int sum = 0;
        line[strcspn(line, "\n")] = '\0';
        char *token = strtok(line, " ");
        while (token != NULL) {
            int res = 0;
            enum status f = str_to_int(token, &res);

            if (f == OVERFLOW) {
                char msg[] = "Overflow\n";
                write(STDOUT_FILENO, msg, sizeof(msg));
                return -1;
            } else if (f == INPUT_ERROR) {
                char msg[] = "Data is incorrect\n";
                write(STDOUT_FILENO, msg, sizeof(msg));
                return -2;
            } else {
                sum += res;
            }

            token = strtok(NULL, " ");
        }

        write_sum(sum);
    }

    return 0;
}