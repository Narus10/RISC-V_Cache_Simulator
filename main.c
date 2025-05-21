#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define ADDRESS_SPACE 32
#define TAG_SIZE 20
#define INDEX_SIZE 8
#define OFFSET_SIZE 4
#define NUMBER_OF_BLOCKS 256
#define MEMORY_SIZE 65536  

uint8_t data[NUMBER_OF_BLOCKS];
uint32_t tag[NUMBER_OF_BLOCKS];
uint8_t valid[NUMBER_OF_BLOCKS];

uint8_t memory[MEMORY_SIZE]; 

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


void displayMemory() {
    printf("Location \t Contents \n");
    printf("----------------------\n"); 
    for(int i = 0; i < MEMORY_SIZE; i++) {
        printf("0x%02x\t\t\t%u\n", i, memory[i]);
        printf("----------------------\n"); 
    }
}

void displayMemoryDebug() {
    printf("Location \t Contents \n");
    printf("----------------------\n"); 
    for(int i = 0; i < 20; i++) {
        printf("0x%02x\t\t\t0x%02x\n", i, memory[i]);
        printf("----------------------\n"); 
    }
}

void randomizeMemoryContent() {
    srand(time(NULL));
    for(int i = 0; i < MEMORY_SIZE; i++) {
        uint8_t randomByte = rand() % 256;
        memory[i] = randomByte;
    }
}

uint32_t get_mask(int bits) {
    return (1U << bits) - 1;
}


void displayCache() {
    printf("\tValid\tTag\t\tData\n");
    for(int i = 0; i < NUMBER_OF_BLOCKS; i++) {
        printf("%d\t", i);
        printf("%d\t\t%d\t\t0x%02x\n", valid[i], tag[i], data[i]);
    }
}

void load_byte(uint32_t address) {
    
    uint32_t offset_mask = get_mask(OFFSET_SIZE);
    uint32_t index_mask  = get_mask(INDEX_SIZE);

    int offset = address & offset_mask;
    int index  = (address >> OFFSET_SIZE) & index_mask;
    int tag    = address >> (OFFSET_SIZE + INDEX_SIZE);

   if (valid[index] && tag[index] == addressTag) {
        printf("Cache Hit!\n");
    } else {
        printf("Cache Miss!, Updating Cache.\n");
        valid[index] = 1;
        tag[index] = addressTag;
        data[index] = memory[address];
    }
}

int main(int argc, char* argv[argc + 1]) {

    randomizeMemoryContent();
    displayMemoryDebug();

    load_byte(0x00000010);
    load_byte(0x00000010);

    return 0;    
} 