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
        perror("Error opening shared memory");
        exit(EXIT_FAILURE);
    }

    char *shm_ptr = mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("Error mapping shared memory");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_write = sem_open(SEM_WRITE, 0);
    sem_t *sem_read = sem_open(SEM_READ, 0);
    if (sem_write == SEM_FAILED || sem_read == SEM_FAILED) {
        perror("Error opening semaphores");
        exit(EXIT_FAILURE);
    }

    int line = 1;
    while (1) {
        sem_wait(sem_read);

        if (shm_ptr[0] == '\0') {
            break;
        }

        remove_carriage_return(shm_ptr);

        char *token = strtok(shm_ptr, " ");
        int line_sum = 0;
        int valid_line = 1;

        while (token != NULL) {
            int num;
            if (!str_to_int(token, &num)) {
                fprintf(stderr, "Invalid number in line %d: %s\n", line, token);
                valid_line = 0;
                break;
            }
            line_sum += num;
            token = strtok(NULL, " ");
        }

        if (valid_line) {
            printf("Sum in line %d = %d\n", line, line_sum);
        }

        line++;
        sem_post(sem_write);
    }

    munmap(shm_ptr, BUF_SIZE);
    sem_close(sem_write);
    sem_close(sem_read);

    return 0;
}
