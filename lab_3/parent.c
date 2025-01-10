#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/wait.h>
#define SHM_NAME "/shared_memory"
#define SEM_WRITE "/sem_write"
#define SEM_READ "/sem_read"
#define BUF_SIZE 256
void write_error(const char *msg) {
    write(STDERR_FILENO, msg, strlen(msg));
}
int main(int argc, char *argv[]) {
    if (argc != 2) {
        const char msg[] = "Usage: ./parent <filename>\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        write_error("Error opening file\n");
        exit(EXIT_FAILURE);
    }
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        write_error("Error creating shared memory\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    if (ftruncate(shm_fd, BUF_SIZE) == -1) {
        write_error("Error setting shared memory size\n");
        fclose(file);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }
    char *shm_ptr = mmap(0, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd,
                         0);
    if (shm_ptr == MAP_FAILED) {
        write_error("Error mapping shared memory\n");
        fclose(file);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }
    sem_t *sem_write = sem_open(SEM_WRITE, O_CREAT, 0666, 1);
    sem_t *sem_read = sem_open(SEM_READ, O_CREAT, 0666, 0);
    if (sem_write == SEM_FAILED || sem_read == SEM_FAILED) {
        write_error("Error creating semaphores\n");
        fclose(file);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }
    pid_t pid = fork();
    if (pid == 0) {
        execl("./child", "./child", NULL);
        write_error("Error executing child process\n");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        char buffer[BUF_SIZE];
        while (fgets(buffer, sizeof(buffer), file) != NULL) {
            sem_wait(sem_write);
            strncpy(shm_ptr, buffer, BUF_SIZE);
            sem_post(sem_read);
        }
        sem_wait(sem_write);
        shm_ptr[0] = '\0';
        sem_post(sem_read);
        wait(NULL);
        fclose(file);
        munmap(shm_ptr, BUF_SIZE);
        shm_unlink(SHM_NAME);
        sem_close(sem_write);
        sem_close(sem_read);
        sem_unlink(SEM_WRITE);
        sem_unlink(SEM_READ);
    }
    return 0;
}