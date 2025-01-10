#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>

#define SHM_NAME "/shared_memory"
#define SEM_WRITE "/sem_write"
#define SEM_READ "/sem_read"
#define BUF_SIZE 256

void write_error(const char *msg) {
    write(STDERR_FILENO, msg, strlen(msg));
}

void write_message(const char *msg, int line, int sum) {
    char buffer[BUF_SIZE];
    int len = snprintf(buffer, BUF_SIZE, msg, line, sum);
    write(STDOUT_FILENO, buffer, len);
}

int str_to_int(const char *str, int *num) {
    char *endptr;
    long val = strtol(str, &endptr, 10);
    if (str == endptr || *endptr != '\0') {
        return 0;
    }
    *num = (int)val;
    return 1;
}

void remove_carriage_return(char *str) {
    char *pos;
    if ((pos = strchr(str, '\n')) != NULL) {
        *pos = '\0';
    }
    if ((pos = strchr(str, '\r')) != NULL) {
        *pos = '\0';
    }
}

int main() {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        write_error("Error opening shared memory\n");
        exit(EXIT_FAILURE);
    }

    char *shm_ptr = mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        write_error("Error mapping shared memory\n");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_write = sem_open(SEM_WRITE, 0);
    sem_t *sem_read = sem_open(SEM_READ, 0);
    if (sem_write == SEM_FAILED || sem_read == SEM_FAILED) {
        write_error("Error opening semaphores\n");
        exit(EXIT_FAILURE);
    }

    int line = 1;

    while (1) {
        sem_wait(sem_read);

        if (shm_ptr[0] == '\0') {
            break;
        }

        remove_carriage_return(shm_ptr);

        char buffer[BUF_SIZE];
        strncpy(buffer, shm_ptr, BUF_SIZE);
        buffer[BUF_SIZE - 1] = '\0';

        char *token = strtok(buffer, " ");
        int line_sum = 0;
        int valid_line = 1;

        while (token != NULL) {
            int num;
            if (!str_to_int(token, &num)) {
                char err_buf[BUF_SIZE];
                int len = snprintf(err_buf, BUF_SIZE, "Invalid number in line %d: %s\n", line, token);
                write(STDERR_FILENO, err_buf, len);
                valid_line = 0;
            } else {
                line_sum += num;
            }
            token = strtok(NULL, " ");
        }

        if (valid_line) {
            write_message("Sum in line %d = %d\n", line, line_sum);
        }

        line++;
        sem_post(sem_write);
    }

    munmap(shm_ptr, BUF_SIZE);
    sem_close(sem_write);
    sem_close(sem_read);
    sem_unlink(SEM_WRITE);
    sem_unlink(SEM_READ);

    return 0;
}
