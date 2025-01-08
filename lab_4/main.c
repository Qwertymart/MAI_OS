#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <time.h>

typedef struct {
    void *(*allocator_create)(void *const memory, const size_t size);
    void (*allocator_destroy)(void *const allocator);
    void *(*allocator_alloc)(void *const allocator, const size_t size);
    void (*allocator_free)(void *const allocator, void *const memory, size_t size);
} AllocatorAPI;

void* fallback_allocator_create(void *const memory, const size_t size) {
    printf("%li\n", size);
    return memory;
}

void fallback_allocator_destroy(void *const allocator) {
    if (allocator)
        printf("\n");
}

void* fallback_allocator_alloc(void *const allocator, const size_t size) {
    printf("%li\n", size);
    if (allocator)
        printf("\n");
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void fallback_allocator_free(void *const allocator, void *const memory, size_t size) {
    munmap(memory, 4096);
    if (allocator)
        printf("\n");
}

int main(int argc, char *argv[]) {
    void *handle = NULL;
    AllocatorAPI api;


    if (argc > 1) {
        handle = dlopen(argv[1], RTLD_LAZY);
        if (!handle) {
            fprintf(stderr, "%s\n", dlerror());
            exit(EXIT_FAILURE);
        }

        api.allocator_create = dlsym(handle, "allocator_create");
        api.allocator_destroy = dlsym(handle, "allocator_destroy");
        api.allocator_alloc = dlsym(handle, "allocator_alloc");
        api.allocator_free = dlsym(handle, "allocator_free");

        if (!api.allocator_create || !api.allocator_destroy || !api.allocator_alloc || !api.allocator_free) {
            fprintf(stderr, "%s\n", dlerror());
            exit(EXIT_FAILURE);
        }
    } else {
        api.allocator_create = fallback_allocator_create;
        api.allocator_destroy = fallback_allocator_destroy;
        api.allocator_alloc = fallback_allocator_alloc;
        api.allocator_free = fallback_allocator_free;
    }

    size_t memory_size = 4096;
    size_t size_data = 128;

    void *memory = mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    void *allocator = api.allocator_create(memory, memory_size);

    clock_t start, end;
    double cpu_time_used;

    start = clock();
    void *ptr1 = api.allocator_alloc(allocator, size_data);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time to allocate %li bytes: %f seconds\n", size_data, cpu_time_used);

    start = clock();
    api.allocator_free(allocator, ptr1, size_data);
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Time to free %li bytes: %f seconds\n", size_data, cpu_time_used);

    api.allocator_destroy(allocator);

    munmap(memory, memory_size);

    if (handle) {
        dlclose(handle);
    }

    return 0;
}


