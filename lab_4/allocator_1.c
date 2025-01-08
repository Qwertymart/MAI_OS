#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define MIN_BLOCK_SIZE 16

typedef struct {
    unsigned char *memory_region;
    size_t total_memory_size;
    size_t total_blocks;
    unsigned char *allocation_map;
    size_t map_size;
} MemoryAllocator;

static size_t calculate_allocation_map_size(size_t block_count) {
    return (block_count + 7) / 8;
}

MemoryAllocator *allocator_create(void *const memory, size_t requested_size) {
    if (requested_size == 0) {
        fprintf(stderr, "Error: Requested size is zero\n");
        return NULL;
    }

    requested_size = (requested_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    size_t block_count = requested_size / MIN_BLOCK_SIZE;
    size_t allocation_map_size = calculate_allocation_map_size(block_count);

    MemoryAllocator *allocator = mmap(NULL, sizeof(MemoryAllocator), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator == MAP_FAILED) {
        perror("Error mmap for allocator structure");
        return NULL;
    }

    allocator->memory_region = mmap(NULL, requested_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator->memory_region == MAP_FAILED) {
        perror("Error mmap for memory region");
        munmap(allocator, sizeof(MemoryAllocator));
        return NULL;
    }

    allocator->allocation_map = mmap(NULL, allocation_map_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (allocator->allocation_map == MAP_FAILED) {
        perror("Error mmap for allocation map");
        munmap(allocator->memory_region, requested_size);
        munmap(allocator, sizeof(MemoryAllocator));
        return NULL;
    }

    allocator->total_memory_size = requested_size;
    allocator->total_blocks = block_count;
    allocator->map_size = allocation_map_size;
    memset(allocator->allocation_map, 0xFF, allocation_map_size);

    return allocator;
}

void *allocator_alloc(MemoryAllocator *allocator, size_t size) {
    if (!allocator || size == 0) {
        fprintf(stderr, "Error: Invalid allocation request\n");
        return NULL;
    }

    if (size > allocator->total_memory_size) {
        fprintf(stderr, "Error: Requested size exceeds total memory\n");
        return NULL;
    }

    size_t required_blocks = (size + MIN_BLOCK_SIZE - 1) / MIN_BLOCK_SIZE;

    for (size_t i = 0; i < allocator->total_blocks - required_blocks + 1;) {
        size_t j;
        for (j = 0; j < required_blocks; j++) {
            size_t byte_index = (i + j) / 8;
            size_t bit_offset = (i + j) % 8;
            if (!(allocator->allocation_map[byte_index] & (1 << bit_offset))) {
                break;
            }
        }

        if (j == required_blocks) {
            for (j = 0; j < required_blocks; j++) {
                size_t byte_index = (i + j) / 8;
                size_t bit_offset = (i + j) % 8;
                allocator->allocation_map[byte_index] &= ~(1 << bit_offset);
            }
            return allocator->memory_region + i * MIN_BLOCK_SIZE;
        }
        i += j + 1;
    }

    fprintf(stderr, "Error: Not enough free memory\n");
    return NULL;
}

void allocator_free(MemoryAllocator *allocator, void *ptr, size_t size) {
    if (!allocator || !ptr || size == 0) {
        fprintf(stderr, "Error: Invalid free request\n");
        return;
    }

    unsigned char *aligned_ptr = (unsigned char *)ptr;
    if (aligned_ptr < allocator->memory_region ||
        aligned_ptr >= allocator->memory_region + allocator->total_memory_size) {
        fprintf(stderr, "Error: Pointer out of allocator bounds\n");
        return;
    }

    if ((aligned_ptr - allocator->memory_region) % MIN_BLOCK_SIZE != 0) {
        fprintf(stderr, "Error: Pointer is not aligned to block boundary\n");
        return;
    }

    size_t blocks_to_free = (size + MIN_BLOCK_SIZE - 1) / MIN_BLOCK_SIZE;
    size_t starting_block_index = (aligned_ptr - allocator->memory_region) / MIN_BLOCK_SIZE;

    if (starting_block_index >= allocator->total_blocks ||
        starting_block_index + blocks_to_free > allocator->total_blocks) {
        fprintf(stderr, "Error: Blocks to free exceed memory bounds\n");
        return;
    }

    for (size_t i = starting_block_index; i < starting_block_index + blocks_to_free; i++) {
        allocator->allocation_map[i / 8] |= (1 << (i % 8));
    }
}

void allocator_destroy(MemoryAllocator *allocator) {
    if (!allocator) {
        fprintf(stderr, "Error: Attempt to destroy a non-existent allocator\n");
        return;
    }

    if (allocator->allocation_map) {
        munmap(allocator->allocation_map, allocator->map_size);
        allocator->allocation_map = NULL;
    }

    if (allocator->memory_region) {
        munmap(allocator->memory_region, allocator->total_memory_size);
        allocator->memory_region = NULL;
    }

    munmap(allocator, sizeof(MemoryAllocator));
}
