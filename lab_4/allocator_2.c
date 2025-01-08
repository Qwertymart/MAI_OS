#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>

typedef struct BuddyAllocator {
    void *memory_region;
    size_t total_memory_size;
    size_t min_block_size;
    size_t max_block_size;
    int max_levels;
    void **free_lists;
} BuddyAllocator;

BuddyAllocator *allocator_create(void *const memory, const size_t size) {
    size_t total_size = pow(2, ceil(log2(size)));
    size_t min_block_size = 64;
    size_t max_block_size = total_size;
    int max_levels = log2(max_block_size / min_block_size) + 1;

    BuddyAllocator *allocator = mmap(NULL, sizeof(BuddyAllocator),
                                     PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    allocator->memory_region = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator->memory_region == MAP_FAILED) {
        perror("mmap");
        munmap(allocator, sizeof(BuddyAllocator));
        return NULL;
    }

    allocator->total_memory_size = total_size;
    allocator->min_block_size = min_block_size;
    allocator->max_block_size = max_block_size;
    allocator->max_levels = max_levels;

    allocator->free_lists = mmap(NULL, max_levels * sizeof(void *),
                                 PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator->free_lists == MAP_FAILED) {
        perror("mmap");
        munmap(allocator->memory_region, total_size);
        munmap(allocator, sizeof(BuddyAllocator));
        return NULL;
    }

    memset(allocator->free_lists, 0, max_levels * sizeof(void *));
    allocator->free_lists[max_levels - 1] = allocator->memory_region;

    return allocator;
}

void *allocator_alloc(BuddyAllocator *const allocator, const size_t size) {
    size_t adjusted_size = size < allocator->min_block_size ? allocator->min_block_size : size;
    int level = log2(allocator->max_block_size / adjusted_size);

    for (int i = level; i < allocator->max_levels; i++) {
        if (allocator->free_lists[i] != NULL) {
            void *block = allocator->free_lists[i];
            allocator->free_lists[i] = *(void **)block;

            while (i > level) {
                i--;
                void *buddy = (void *)((char *)block + (allocator->min_block_size << i));
                *(void **)buddy = allocator->free_lists[i];
                allocator->free_lists[i] = buddy;
            }

            return block;
        }
    }

    return NULL;
}

void allocator_destroy(BuddyAllocator *const allocator) {
    munmap(allocator->free_lists, allocator->max_levels * sizeof(void *));
    munmap(allocator->memory_region, allocator->total_memory_size);
    munmap(allocator, sizeof(BuddyAllocator));
}

void allocator_free(BuddyAllocator *const allocator, void *const memory) {
    size_t size = allocator->min_block_size;
    int level = log2(allocator->max_block_size / size);
    void *ptr = memory;

    while (level < allocator->max_levels - 1) {
        void *buddy = (void *)((uintptr_t)ptr ^ (allocator->min_block_size << level));
        void **prev = &allocator->free_lists[level];
        while (*prev != NULL && *prev != buddy) {
            prev = (void **)*prev;
        }

        if (*prev == buddy) {
            *prev = *(void **)buddy;
            ptr = (ptr < buddy) ? ptr : buddy;
            level++;
        } else {
            break;
        }
    }

    *(void **)ptr = allocator->free_lists[level];
    allocator->free_lists[level] = ptr;
}
