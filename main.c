#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Constants
#define ADDRESS_SPACE_BITS 32
#define TAG_SIZE_BITS 20
#define INDEX_SIZE_BITS 8
#define OFFSET_SIZE_BITS 4
#define NUMBER_OF_BLOCKS 256
#define MEMORY_SIZE 65536  

// Cache line structure
typedef struct {
    uint8_t valid;
    uint32_t tag;
    uint8_t data;
} CacheBlock;

// Global memory and cache
uint8_t memory[MEMORY_SIZE];
CacheBlock cache[NUMBER_OF_BLOCKS];

// Utility: generate a bitmask for a given number of bits
uint32_t get_mask(int bits) {
    return (1U << bits) - 1;
}

// Randomize memory with byte values [0, 255]
void randomize_memory_content(void) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < MEMORY_SIZE; ++i) {
        memory[i] = (uint8_t)(rand() % 256);
    }
}

// Display full memory (takes time if the terminal is slow, its buffered... use debug function to be faster)
void display_memory(void) {
    printf("Location\tContents\n");
    printf("--------------------------\n");
    for (int i = 0; i < MEMORY_SIZE; ++i) {
        printf("0x%04X\t\t0x%02X\n", i, memory[i]);
    }
}

// Display first 20 bytes of memory for debug
void display_memory_debug(void) {
    printf("Memory (First 20 Bytes):\n");
    printf("Location\tContents\n");
    printf("--------------------------\n");
    for (int i = 0; i < 20; ++i) {
        printf("0x%04X\t\t0x%02X\n", i, memory[i]);
    }
}

// Display current cache state
void display_cache(void) {
    printf("Index\tValid\tTag\t\tData\n");
    printf("------------------------------------\n");
    for (int i = 0; i < NUMBER_OF_BLOCKS; ++i) {
        printf("%d\t%d\t0x%05X\t0x%02X\n", i, cache[i].valid, cache[i].tag, cache[i].data);
    }
}

// Load a byte from memory using direct-mapped cache
void load_byte(uint32_t address) {
    uint32_t offset_mask = get_mask(OFFSET_SIZE_BITS);
    uint32_t index_mask  = get_mask(INDEX_SIZE_BITS);

    uint32_t offset = address & offset_mask;
    uint32_t index = (address >> OFFSET_SIZE_BITS) & index_mask;
    uint32_t address_tag = address >> (OFFSET_SIZE_BITS + INDEX_SIZE_BITS);

    printf("Accessing address: 0x%08X\n", address);
    printf(" -> Index: %u | Tag: 0x%05X | Offset: %u\n", index, address_tag, offset);

    if (cache[index].valid && cache[index].tag == address_tag) {
        printf("Cache Hit! Data: 0x%02X\n", cache[index].data);
    } else {
        printf("Cache Miss! Updating cache.\n");
        cache[index].valid = 1;
        cache[index].tag = address_tag;
        cache[index].data = memory[address];
        printf("Loaded Data: 0x%02X\n", cache[index].data);
    }
}

// Entry point
int main(void) {
    randomize_memory_content();
    display_memory_debug();

    load_byte(0x00000010);
    load_byte(0x00000010);

    printf("\nFinal Cache State:\n");
    display_cache();

    return 0;
}
