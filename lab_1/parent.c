#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

void error(const char* msg)
{
    write(STDERR_FILENO, msg, strlen(msg));
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        const char msg[] = "Usage: %s <filename>\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }

    FILE *file = fopen(argv[1], "r");
    if (file == NULL) {
        error("Error opening file");
        exit(EXIT_FAILURE);
    }

    int chanel[2];
    if (pipe(chanel) == -1) {
        error("Pipe failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid < 0) {
        error("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {

        close(chanel[1]);

        dup2(chanel[0], STDIN_FILENO);
        close(chanel[0]);

        execv("./b","./b", NULL);
        error("execlp failed");
        exit(EXIT_FAILURE);
    } else {
        close(chanel[0]);

        char line[256];
        while (fgets(line, sizeof(line), file) != NULL) {
            write(chanel[1], line, strlen(line));
        }
        close(chanel[1]);
        fclose(file);

        wait(NULL);
    }

    return 0;
}
