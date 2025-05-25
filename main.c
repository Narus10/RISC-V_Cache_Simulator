#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h> 
#include <limits.h> // For UINT32_MAX

// --- Enum Definitions ---
typedef enum { LRU, FIFO, RANDOM } ReplacementPolicy;
typedef enum { WRITE_THROUGH, WRITE_BACK } WriteHitPolicy;
typedef enum { WRITE_ALLOCATE, NO_WRITE_ALLOCATE } WriteMissPolicy;

// --- Cache Line Structure ---
typedef struct {
    uint8_t valid;
    uint32_t tag;
    uint8_t* data; 
    uint64_t last_access_timestamp; 
    uint64_t loaded_timestamp;    
    uint8_t dirty; 
} CacheBlock;

// --- Preset Structure Definition ---
typedef struct {
    const char* name;
    uint32_t memory_size;
    uint32_t address_space_bits;
    uint32_t tag_size_bits;
    uint32_t index_size_bits;
    uint32_t offset_size_bits;
    ReplacementPolicy repl_policy; 
    WriteHitPolicy write_hit_policy;
    WriteMissPolicy write_miss_policy;
} CacheConfigPreset;

// --- Forward Declarations ---
void report_error(const char* message, int exit_code_if_nonzero);
bool is_power_of_two(uint32_t n);
uint32_t get_mask(uint32_t bits);
void apply_preset(const CacheConfigPreset* preset);
void display_current_config(void);
void randomize_memory_content(void);
void display_memory_formatted(uint32_t start_address, uint32_t end_address, uint8_t bytes_per_line);
void display_cache(void);
void write_block_to_memory_on_eviction(uint32_t cache_index);
uint8_t perform_byte_load_for_cache(uint32_t address, bool is_internal_call);
void load_byte(uint32_t address);
uint16_t load_word(uint32_t address);
uint32_t load_double_word(uint32_t address);
void store_byte(uint32_t address, uint8_t value);

// --- Global State & Configuration ---
uint64_t global_timestamp_counter = 0;
ReplacementPolicy current_replacement_policy = LRU; 
WriteHitPolicy current_write_hit_policy = WRITE_THROUGH; 
WriteMissPolicy current_write_miss_policy = NO_WRITE_ALLOCATE;

CacheConfigPreset common_presets[] = {
    { "Small (LRU, WT, NWA)", 256, 8, 3, 3, 2, LRU, WRITE_THROUGH, NO_WRITE_ALLOCATE },
    { "Default (LRU, WT, NWA)", 65536, 32, 20, 8, 4, LRU, WRITE_THROUGH, NO_WRITE_ALLOCATE },
    { "Write-Back Example (LRU, WB, WA)", 65536, 32, 20, 8, 4, LRU, WRITE_BACK, WRITE_ALLOCATE }
};
size_t num_common_presets = sizeof(common_presets) / sizeof(common_presets[0]);

uint32_t ADDRESS_SPACE_BITS;
uint32_t TAG_SIZE_BITS;
uint32_t INDEX_SIZE_BITS;
uint32_t OFFSET_SIZE_BITS;
uint32_t NUMBER_OF_BLOCKS; 
uint32_t MEMORY_SIZE;    
uint32_t BYTES_PER_BLOCK; 

uint8_t *memory = NULL;
CacheBlock *cache = NULL;

#define WORD_SIZE_BYTES 2
#define DOUBLE_WORD_SIZE_BYTES 4

// --- Helper Function Definitions ---
void report_error(const char* message, int exit_code_if_nonzero) {
    fprintf(stderr, "Error: %s\n", message);
    if (exit_code_if_nonzero != 0) {
        if (cache != NULL && NUMBER_OF_BLOCKS > 0 && BYTES_PER_BLOCK > 0) { // Added BYTES_PER_BLOCK check
            for (uint32_t i = 0; i < NUMBER_OF_BLOCKS; ++i) { // Check NUMBER_OF_BLOCKS before loop
                if (cache[i].data != NULL) free(cache[i].data);
            }
            free(cache); cache = NULL;
        }
        if (memory != NULL) {free(memory); memory = NULL;}
        exit(exit_code_if_nonzero);
    }
}

bool is_power_of_two(uint32_t n) {
    if (n == 0) return false;
    return (n & (n - 1)) == 0;
}

uint32_t get_mask(uint32_t bits) {
    if (bits == 0) return 0; 
    if (bits >= 32) return 0xFFFFFFFF; 
    return (1U << bits) - 1;
}

// --- Core Logic Function Definitions ---
void apply_preset(const CacheConfigPreset* preset) {
    if (preset == NULL) { report_error("Null preset provided.", 0); return; }
    MEMORY_SIZE = preset->memory_size;
    ADDRESS_SPACE_BITS = preset->address_space_bits;
    TAG_SIZE_BITS = preset->tag_size_bits;
    INDEX_SIZE_BITS = preset->index_size_bits;
    OFFSET_SIZE_BITS = preset->offset_size_bits;
    current_replacement_policy = preset->repl_policy;
    current_write_hit_policy = preset->write_hit_policy;
    current_write_miss_policy = preset->write_miss_policy;

    const char* r_pol_names[] = {"LRU", "FIFO", "Random"};
    const char* wh_pol_names[] = {"Write-Through", "Write-Back"};
    const char* wm_pol_names[] = {"Write-Allocate", "No-Write-Allocate"};

    printf("\nApplied preset: \"%s\"\n", preset->name);
    printf("  Config: MEM_SIZE:%u ADDR_BITS:%u TAG_BITS:%u IDX_BITS:%u OFF_BITS:%u\n",
           MEMORY_SIZE, ADDRESS_SPACE_BITS, TAG_SIZE_BITS, INDEX_SIZE_BITS, OFFSET_SIZE_BITS);
    printf("  Policies: Repl: %s, WriteHit: %s, WriteMiss: %s\n",
           r_pol_names[current_replacement_policy], wh_pol_names[current_write_hit_policy], wm_pol_names[current_write_miss_policy]);
}

void display_current_config(void) {
    const char* r_pol_names[] = {"LRU", "FIFO", "Random"};
    const char* wh_pol_names[] = {"Write-Through", "Write-Back"};
    const char* wm_pol_names[] = {"Write-Allocate", "No-Write-Allocate"};

    printf("\n--- Current Configuration ---\n");
    printf("Memory Size: %u bytes (0x%08X)\n", MEMORY_SIZE, MEMORY_SIZE);
    printf("Address Space: %u bits\n", ADDRESS_SPACE_BITS);
    printf("Tag Size: %u bits\n", TAG_SIZE_BITS);
    printf("Index Size: %u bits\n", INDEX_SIZE_BITS);
    printf("Offset Size: %u bits\n", OFFSET_SIZE_BITS);
    printf("------------------------------\n");
    printf("Number of Cache Blocks: %u\n", NUMBER_OF_BLOCKS);
    printf("Bytes Per Block: %u\n", BYTES_PER_BLOCK);
    printf("------------------------------\n");
    printf("Replacement Policy: %s\n", r_pol_names[current_replacement_policy]);
    printf("Write Hit Policy: %s\n", wh_pol_names[current_write_hit_policy]);
    printf("Write Miss Policy: %s\n", wm_pol_names[current_write_miss_policy]);
    printf("------------------------------\n");
}


void randomize_memory_content(void) {
    if (memory == NULL || MEMORY_SIZE == 0) return;
    // srand((unsigned int)time(NULL)); // Seed once in main
    for (uint32_t i = 0; i < MEMORY_SIZE; ++i) memory[i] = (uint8_t)(rand() % 256);
}

void display_memory_formatted(uint32_t start_address, uint32_t end_address, uint8_t bytes_per_line) {
    char msg_buffer[256];
    uint32_t temp_end_address = end_address; 

    if (memory == NULL && MEMORY_SIZE > 0) {
         report_error("Memory not allocated but MEMORY_SIZE > 0.", 0); return;
    }
    if (MEMORY_SIZE == 0) { 
        printf("\nMemory View: MEMORY_SIZE is 0. Nothing to display.\n"); return;
    }
    if (bytes_per_line == 0) {
        report_error("bytes_per_line cannot be 0.", 0); return;
    }
     if (start_address >= MEMORY_SIZE) { 
        snprintf(msg_buffer, sizeof(msg_buffer),"start_address (0x%08X) is out of bounds (MEM_SIZE 0x%08X).", start_address, MEMORY_SIZE);
        report_error(msg_buffer, 0); return;
    }
    
    if (temp_end_address >= MEMORY_SIZE) { 
        temp_end_address = MEMORY_SIZE - 1;
    }

     if (start_address > temp_end_address) { 
        printf("\nMemory View (0x%08X - 0x%08X): Invalid range or nothing to display.\n", start_address, temp_end_address);
        return;
    }


    printf("\nMemory View (0x%08X - 0x%08X), %u bytes/line:\n", start_address, temp_end_address, bytes_per_line);
    printf("----------------------------------------------------------\n");
    for (uint32_t current_base = start_address; current_base <= temp_end_address; ) {
        printf("0x%08X: ", current_base);
        for (uint8_t i = 0; i < bytes_per_line; ++i) {
            uint32_t current_address = current_base + i;
            if (current_address <= temp_end_address) {
                printf("%02X ", memory[current_address]);
            } else {
                printf("   "); 
            }
            if (i > 0 && (i + 1) % 8 == 0 && i < bytes_per_line - 1 && bytes_per_line > 8) printf(" ");
        }
        printf("\n");
        if (current_base > UINT32_MAX - bytes_per_line) { break; } 
        current_base += bytes_per_line;
         if (current_base < start_address && current_base != 0) { break; } 
    }
    printf("----------------------------------------------------------\n");
}

void display_cache(void) {
    printf("\nIdx\tV\tD\tTag\t\tLRU TS\tFIFO TS\tData (Hex bytes)\n");
    printf("---------------------------------------------------------------------------------------------------------------------------\n");
    if (cache == NULL || NUMBER_OF_BLOCKS == 0 || BYTES_PER_BLOCK == 0) {
        report_error("Cache not allocated, empty, or block size is zero. Cannot display.", 0); return;
    }
    for (uint32_t i = 0; i < NUMBER_OF_BLOCKS; ++i) {
        printf("%-3u\t%d\t%d\t0x%0*X\t", 
               i, cache[i].valid, cache[i].dirty,
               (int)(TAG_SIZE_BITS / 4 + (TAG_SIZE_BITS % 4 != 0)), cache[i].tag);
        if (cache[i].valid) {
            printf("%-6lu\t%-6lu\t", cache[i].last_access_timestamp, cache[i].loaded_timestamp);
        } else {
            printf("%-6s\t%-6s\t", "-", "-");
        }
        if (cache[i].data != NULL) { 
            for (uint32_t j=0; j<BYTES_PER_BLOCK; ++j) {
                if(cache[i].valid) printf("%02X ", cache[i].data[j]); else printf("-- ");
                if (BYTES_PER_BLOCK>8 && (j+1)%8==0 && j<BYTES_PER_BLOCK-1) printf("  ");
            }
        } else { printf("Data pointer NULL!");}
        printf("\n");
    }
    printf("---------------------------------------------------------------------------------------------------------------------------\n");
}

void write_block_to_memory_on_eviction(uint32_t cache_index) {
    if (!cache[cache_index].valid || !cache[cache_index].dirty) return;
    if (memory == NULL) { report_error("WB Eviction: Memory not allocated.", 0); return; }

    uint32_t tag_shift = INDEX_SIZE_BITS + OFFSET_SIZE_BITS;
    uint32_t mem_addr = (cache[cache_index].tag << tag_shift) | (cache_index << OFFSET_SIZE_BITS);
    
    printf("Evicting: Writing back dirty block at index %u (Tag 0x%X, MemAddr 0x%08X)\n", 
           cache_index, cache[cache_index].tag, mem_addr);

    if (mem_addr + BYTES_PER_BLOCK <= MEMORY_SIZE) {
        memcpy(&memory[mem_addr], cache[cache_index].data, BYTES_PER_BLOCK);
    } else {
      char err[200]; snprintf(err,sizeof(err),"WB Eviction: Addr 0x%08X OOB.", mem_addr); report_error(err,0);
    }
    cache[cache_index].dirty = 0;
}

uint8_t perform_byte_load_for_cache(uint32_t address, bool is_internal_call) {
    global_timestamp_counter++;
    uint8_t return_value = 0xFF; 
    char msg_buffer[256];

    if (MEMORY_SIZE==0){snprintf(msg_buffer,sizeof(msg_buffer),"Load Error: MEM_SIZE 0 for addr 0x%X",address); report_error(msg_buffer,0); return val;}
    if (!memory){report_error("Load Error: Memory NULL",1); return val;}
    if (address>=MEMORY_SIZE){snprintf(msg_buffer,sizeof(msg_buffer),"Load Error: Addr 0x%X OOB (MEM_SIZE 0x%X)",address,MEMORY_SIZE); report_error(msg_buffer,0); return val;}
    if (NUMBER_OF_BLOCKS==0 || BYTES_PER_BLOCK==0){ if(!is_internal_call) printf("No cache/Invalid block size, direct mem access.\n"); return memory[address];}
    if (!cache){report_error("Load Error: Cache NULL",1); return val;}

    uint32_t off_mask=get_mask(OFFSET_SIZE_BITS); 
    uint32_t idx = (address >> OFFSET_SIZE_BITS) & get_mask(INDEX_SIZE_BITS);
    uint32_t off = address & off_mask;
    uint32_t ad_tag = address >> (OFFSET_SIZE_BITS + INDEX_SIZE_BITS);

    if(!is_internal_call){printf("Accessing address for byte load: 0x%08X\n", address); printf(" -> Index: %u | Tag: 0x%0*X | Offset: %u\n",idx,(int)(TAG_SIZE_BITS/4+(TAG_SIZE_BITS%4!=0)),ad_tag,off);}
    CacheBlock* blk = &cache[idx];
    if(!blk->data){report_error("Load Error: Cache block data NULL",1); return val;}

    if(blk->valid && blk->tag == ad_tag){ // Hit
        val = blk->data[off];
        if(current_replacement_policy == LRU) blk->last_access_timestamp = global_timestamp_counter;
        if(!is_internal_call) printf("Cache Hit! Data:0x%02X (LRU TS:%lu)\n",val,blk->last_access_timestamp);
    } else { // Miss
        if(!is_internal_call) printf("Cache Miss for address 0x%08X. ",address);
        if(blk->valid && blk->dirty) write_block_to_memory_on_eviction(idx);
        else if(blk->valid && !is_internal_call) printf("Replacing clean block (Old Tag:0x%X).\n",blk->tag);
        else if(!is_internal_call) printf("Loading into empty block.\n");
        
        uint32_t blk_start = address & ~off_mask;
        if(blk_start + BYTES_PER_BLOCK > MEMORY_SIZE){
            snprintf(msg_buffer,sizeof(msg_buffer),"Load Error: Block 0x%X-0x%X OOB.",blk_start, blk_start+BYTES_PER_BLOCK-1); report_error(msg_buffer,0);
            return memory[address]; 
        }
        memcpy(blk->data, &memory[blk_start], BYTES_PER_BLOCK);
        blk->valid=1; blk->tag=ad_tag; blk->dirty=0;
        if(current_replacement_policy==LRU) blk->last_access_timestamp = global_timestamp_counter;
        if(current_replacement_policy==FIFO) blk->loaded_timestamp = global_timestamp_counter;
        val = blk->data[off];
        if(!is_internal_call) printf("Loaded block. Data:0x%02X (LRU TS:%lu FIFO TS:%lu)\n",val,blk->last_access_timestamp,blk->loaded_timestamp);
    }
    return val;
}

void store_byte(uint32_t address, uint8_t value) {
    global_timestamp_counter++; 
    char msg_buffer[256];
    if(MEMORY_SIZE==0){snprintf(msg_buffer,sizeof(msg_buffer),"Store Error: MEM_SIZE 0 for addr 0x%X",address); report_error(msg_buffer,0); return;}
    if(!memory){report_error("Store Error: Memory NULL",1);return;}
    if(address>=MEMORY_SIZE){snprintf(msg_buffer,sizeof(msg_buffer),"Store Error: Addr 0x%X OOB (MEM_SIZE 0x%X)",address,MEMORY_SIZE); report_error(msg_buffer,0); return;}
    
    printf("\nStoring value 0x%02X to address 0x%08X\n", value, address);
    if(NUMBER_OF_BLOCKS==0||BYTES_PER_BLOCK==0){memory[address]=value; printf(" (No Cache - Mem updated)\n"); return;}
    if(!cache){report_error("Store Error: Cache NULL",1);return;}

    uint32_t off_mask=get_mask(OFFSET_SIZE_BITS); 
    uint32_t idx = (address >> OFFSET_SIZE_BITS) & get_mask(INDEX_SIZE_BITS);
    uint32_t off = address & off_mask;
    uint32_t ad_tag = address >> (OFFSET_SIZE_BITS + INDEX_SIZE_BITS);
    printf(" -> Idx:%u Tag:0x%X Off:%u\n", idx, ad_tag, off);
    CacheBlock* blk = &cache[idx];
    if(!blk->data){report_error("Store Error: Cache block data NULL",1);return;}

    if(blk->valid && blk->tag == ad_tag){ // Hit
        printf("Write Hit! ");
        if(current_write_hit_policy==WRITE_THROUGH){ memory[address]=value; blk->data[off]=value; blk->dirty=0; printf("Policy: Write-Through. Mem & Cache updated. Block clean. ");}
        else{ blk->data[off]=value; blk->dirty=1; printf("Policy: Write-Back. Cache updated. Block dirty. ");}
        if(current_replacement_policy==LRU) blk->last_access_timestamp = global_timestamp_counter;
        printf("(LRU TS:%lu)\n",blk->last_access_timestamp);
    } else { // Miss
        printf("Write Miss! ");
        if(current_write_miss_policy==WRITE_ALLOCATE){
            printf("Policy: Write-Allocate.\n");
            if(blk->valid && blk->dirty) write_block_to_memory_on_eviction(idx);
            else if(blk->valid) printf("WA: Replacing clean block (Old Tag:0x%X).\n",blk->tag); else printf("WA: Loading to empty block.\n");
            uint32_t blk_start = address & ~off_mask;
            if(blk_start + BYTES_PER_BLOCK > MEMORY_SIZE){
                snprintf(msg_buffer,sizeof(msg_buffer),"Store/WA Error: Block 0x%X-0x%X OOB.",blk_start,blk_start+BYTES_PER_BLOCK-1); report_error(msg_buffer,0);
                memory[address]=value; printf(" (Block load failed during WA, wrote to memory directly)\n"); return;
            }
            memcpy(blk->data, &memory[blk_start], BYTES_PER_BLOCK);
            blk->valid=1; blk->tag=ad_tag; blk->dirty=0; 
            if(current_replacement_policy==LRU) blk->last_access_timestamp=global_timestamp_counter;
            if(current_replacement_policy==FIFO) blk->loaded_timestamp=global_timestamp_counter;
            printf("WA: Loaded block. Now performing write: ");
            if(current_write_hit_policy==WRITE_THROUGH){ memory[address]=value; blk->data[off]=value; blk->dirty=0; printf("Write-Through on allocated block. Mem & Cache updated. Block clean.\n");}
            else{ blk->data[off]=value; blk->dirty=1; printf("Write-Back on allocated block. Cache updated. Block dirty.\n");}
        } else { memory[address]=value; printf("Policy: No-Write-Allocate. Updated memory only.\n");}
    }
}

void load_byte(uint32_t ad) { perform_byte_load_for_cache(ad, false); }
uint16_t load_word(uint32_t ad){ 
    char msg[128]; printf("\nAttempting to load word from address 0x%08X\n", ad);
    if ((MEMORY_SIZE > 0 && ad >= MEMORY_SIZE - (WORD_SIZE_BYTES -1) && ad < MEMORY_SIZE && WORD_SIZE_BYTES > 0) || (MEMORY_SIZE > 0 && ad >= MEMORY_SIZE) || (MEMORY_SIZE == 0 && ad !=0) || (WORD_SIZE_BYTES > 0 && ad + WORD_SIZE_BYTES < ad) ) { 
        snprintf(msg, sizeof(msg), "Error (load_word): Word access at 0x%08X would read past/outside MEMORY_SIZE (0x%08X).", ad, MEMORY_SIZE);
        report_error(msg, 0); return 0; }
    if (ad % WORD_SIZE_BYTES != 0) { snprintf(msg, sizeof(msg), "Warning (load_word): Unaligned at 0x%08X.", ad); report_error(msg, 0); }
    uint8_t b0=perform_byte_load_for_cache(ad,true), b1=perform_byte_load_for_cache(ad+1,true); 
    uint16_t val = (uint16_t)b0|((uint16_t)b1<<8);
    printf("Word loaded from 0x%08X: 0x%04X (B0:0x%02X B1:0x%02X)\n", ad, val, b0, b1); return val;
}
uint32_t load_double_word(uint32_t ad){ 
    char msg[128]; printf("\nAttempting to load double word from address 0x%08X\n", ad);
    if ((MEMORY_SIZE > 0 && ad >= MEMORY_SIZE - (DOUBLE_WORD_SIZE_BYTES -1) && ad < MEMORY_SIZE && DOUBLE_WORD_SIZE_BYTES > 0) || (MEMORY_SIZE > 0 && ad >= MEMORY_SIZE) || (MEMORY_SIZE == 0 && ad !=0) || (DOUBLE_WORD_SIZE_BYTES > 0 && ad + DOUBLE_WORD_SIZE_BYTES < ad)) {
        snprintf(msg, sizeof(msg), "Error (load_dword): DWord access at 0x%08X would read past/outside MEM_SIZE (0x%08X).", ad, MEMORY_SIZE);
        report_error(msg, 0); return 0; }
    if (ad % DOUBLE_WORD_SIZE_BYTES != 0) { snprintf(msg, sizeof(msg), "Warning (load_dword): Unaligned at 0x%08X.", ad); report_error(msg, 0); }
    uint8_t b0=perform_byte_load_for_cache(ad,true), b1=perform_byte_load_for_cache(ad+1,true);
    uint8_t b2=perform_byte_load_for_cache(ad+2,true), b3=perform_byte_load_for_cache(ad+3,true);
    uint32_t val = (uint32_t)b0|((uint32_t)b1<<8)|((uint32_t)b2<<16)|((uint32_t)b3<<24);
    printf("DWord loaded from 0x%08X: 0x%08X (B0:0x%02X B1:0x%02X B2:0x%02X B3:0x%02X)\n", ad, val, b0,b1,b2,b3); return val;
}

// --- Main Function ---
int main(void) {
    char line_buffer[256]; 
    char *token; char msg_buffer[256];
    const char* r_pol_names[] = {"LRU", "FIFO", "Random"};
    const char* wh_pol_names[] = {"Write-Through", "Write-Back"};
    const char* wm_pol_names[] = {"Write-Allocate", "No-Write-Allocate"};
    
    srand((unsigned int)time(NULL)); 
    apply_preset(&common_presets[1]); // Default to "Original Config"

    printf("Available Cache Presets:\n");
    for (size_t i = 0; i < num_common_presets; ++i) printf("  %zu. %s\n", i + 1, common_presets[i].name);
    printf("Enter preset number (1-%zu) or 0 to use current default: ", num_common_presets);
    int choice; 
    if (scanf("%d", &choice) == 1) {
        while (getchar()!='\n' && getchar()!=EOF); 
        if (choice > 0 && (size_t)choice <= num_common_presets) apply_preset(&common_presets[choice - 1]);
        else if (choice != 0) printf("Invalid preset choice. Using previously set/default.\n");
        else printf("Keeping current default settings.\n");
    } else { while (getchar()!='\n' && getchar()!=EOF); printf("Invalid input. Using current settings.\n");}

    printf("\nRepl Policy (current: %s):\n1.LRU 2.FIFO 3.Random (0=keep): ",r_pol_names[current_replacement_policy]);
    int p_choice; if (scanf("%d", &p_choice)==1){ while(getchar()!='\n'&&getchar()!=EOF); if (p_choice>=1 && p_choice<=3) current_replacement_policy=(ReplacementPolicy)(p_choice-1); else if (p_choice!=0) printf("Invalid. Keeping.\n"); else printf("Keeping.\n");} else {while(getchar()!='\n'&&getchar()!=EOF);printf("Invalid. Keeping.\n");}
    printf("Repl Policy: %s\n", r_pol_names[current_replacement_policy]);

    printf("Write Hit (current: %s):\n1.WT 2.WB (0=keep): ", wh_pol_names[current_write_hit_policy]);
    if (scanf("%d", &p_choice)==1){ while(getchar()!='\n'&&getchar()!=EOF); if (p_choice>=1 && p_choice<=2) current_write_hit_policy=(WriteHitPolicy)(p_choice-1); else if (p_choice!=0) printf("Invalid. Keeping.\n"); else printf("Keeping.\n");} else {while(getchar()!='\n'&&getchar()!=EOF);printf("Invalid. Keeping.\n");}
    printf("Write Hit: %s\n", wh_pol_names[current_write_hit_policy]);

    printf("Write Miss (current: %s):\n1.WA 2.NWA (0=keep): ", wm_pol_names[current_write_miss_policy]);
    if (scanf("%d", &p_choice)==1){ while(getchar()!='\n'&&getchar()!=EOF); if (p_choice>=1 && p_choice<=2) current_write_miss_policy=(WriteMissPolicy)(p_choice-1); else if (p_choice!=0) printf("Invalid. Keeping.\n"); else printf("Keeping.\n");} else {while(getchar()!='\n'&&getchar()!=EOF);printf("Invalid. Keeping.\n");}
    printf("Write Miss: %s\n", wm_pol_names[current_write_miss_policy]);

    // Validation & Calculation
    if (ADDRESS_SPACE_BITS > 32) report_error("Config Error: ADDRESS_SPACE_BITS > 32 not supported.", 1);
    if (ADDRESS_SPACE_BITS > 0 && (TAG_SIZE_BITS+INDEX_SIZE_BITS+OFFSET_SIZE_BITS != ADDRESS_SPACE_BITS)) report_error("Config Error: T+I+O bits != ADDR_BITS.",1);
    if (ADDRESS_SPACE_BITS == 0 && (TAG_SIZE_BITS !=0 || INDEX_SIZE_BITS !=0 || OFFSET_SIZE_BITS !=0)) report_error("Config Error: Bit sizes must be 0 if ADDR_BITS is 0.",1);
    if (INDEX_SIZE_BITS >= 32 && ADDRESS_SPACE_BITS > 0) report_error("Config Error: INDEX_SIZE_BITS too large.",1);
    
    if (ADDRESS_SPACE_BITS == 0) { NUMBER_OF_BLOCKS=0; BYTES_PER_BLOCK=0; MEMORY_SIZE=0; }
    else {
        NUMBER_OF_BLOCKS = (INDEX_SIZE_BITS==0)?1:(1U<<INDEX_SIZE_BITS);
        if (NUMBER_OF_BLOCKS==0 && INDEX_SIZE_BITS>0 && INDEX_SIZE_BITS < 32) report_error("Config Error: NUMBER_OF_BLOCKS=0 (large INDEX_SIZE_BITS).",1);
        if (OFFSET_SIZE_BITS >= 32) report_error("Config Error: OFFSET_SIZE_BITS too large.",1);
        BYTES_PER_BLOCK = (OFFSET_SIZE_BITS==0)?1:(1U<<OFFSET_SIZE_BITS);
        if (BYTES_PER_BLOCK==0 && OFFSET_SIZE_BITS>0 && OFFSET_SIZE_BITS < 32) report_error("Config Error: BYTES_PER_BLOCK=0 (large OFFSET_SIZE_BITS).",1);
    }
    printf("Derived Values: NUMBER_OF_BLOCKS: %u, BYTES_PER_BLOCK: %u\n", NUMBER_OF_BLOCKS, BYTES_PER_BLOCK);
    if (MEMORY_SIZE > 0 && !is_power_of_two(MEMORY_SIZE)) {snprintf(msg_buffer,sizeof(msg_buffer),"Warning: MEM_SIZE (%u) not power of 2.",MEMORY_SIZE); report_error(msg_buffer,0);}

    // Allocation & Init
    if (MEMORY_SIZE > 0) {
        memory = (uint8_t*)malloc(MEMORY_SIZE);
        if (!memory) report_error("Runtime Error: Main memory alloc failed.",1);
        randomize_memory_content();
    }
    if (NUMBER_OF_BLOCKS > 0 && BYTES_PER_BLOCK > 0) {
        cache = (CacheBlock*)malloc(NUMBER_OF_BLOCKS * sizeof(CacheBlock));
        if (!cache) report_error("Runtime Error: Cache struct alloc failed.",1);
        for (uint32_t i=0; i<NUMBER_OF_BLOCKS; ++i) {
            cache[i].data = (uint8_t*)malloc(BYTES_PER_BLOCK);
            if (!cache[i].data) { snprintf(msg_buffer,sizeof(msg_buffer),"Runtime Error: Cache block %u data failed.",i); report_error(msg_buffer,1);}
            memset(cache[i].data, 0, BYTES_PER_BLOCK);
            cache[i].valid=0; cache[i].tag=0; cache[i].dirty=0;
            cache[i].last_access_timestamp=0; cache[i].loaded_timestamp=0;
        }
    }
    
    printf("\n--- Cache Interactive Mode ---\nType 'help' for commands.\n");
    display_current_config();

    while(1) {
        printf("cache_sim> ");
        fflush(stdout); 
        if (fgets(line_buffer, sizeof(line_buffer), stdin) == NULL) break; 
        line_buffer[strcspn(line_buffer, "\n")] = 0; 
        token = strtok(line_buffer, " \t");
        if (token == NULL) continue;

        if (strcmp(token, "load") == 0) {
            char* addr_str = strtok(NULL, " \t"); char* type_str = strtok(NULL, " \t");
            if (!addr_str) { report_error("Usage: load <addr> [byte|word|dword]",0); continue; }
            uint32_t addr = strtoul(addr_str, NULL, 0);
            if (!type_str || strcmp(type_str, "byte")==0) load_byte(addr);
            else if (strcmp(type_str, "word")==0) load_word(addr);
            else if (strcmp(type_str, "dword")==0) load_double_word(addr);
            else report_error("Invalid load type. Use 'byte', 'word', or 'dword'.",0);
        } else if (strcmp(token, "store") == 0) {
            char* addr_str = strtok(NULL, " \t"); char* val_str = strtok(NULL, " \t");
            if (!addr_str || !val_str) { report_error("Usage: store <addr> <value>",0); continue; }
            uint32_t addr = strtoul(addr_str, NULL, 0);
            uint8_t val = (uint8_t)strtoul(val_str, NULL, 0);
            store_byte(addr, val);
        } else if (strcmp(token, "display") == 0) {
            char* target = strtok(NULL, " \t");
            if (!target) { report_error("Usage: display <cache|memory|config>",0); continue; }
            if (strcmp(target, "cache")==0) display_cache();
            else if (strcmp(target, "config")==0) display_current_config();
            else if (strcmp(target, "memory")==0) {
                char*s=strtok(NULL," \t"); char*e=strtok(NULL," \t"); char*w=strtok(NULL," \t");
                if(!s||!e){report_error("Usage: display memory <start> <end> [width]",0);continue;}
                uint32_t st=strtoul(s,NULL,0); uint32_t ed=strtoul(e,NULL,0);
                uint8_t wd=(w!=NULL)?(uint8_t)strtoul(w,NULL,0):16; if(wd==0)wd=16;
                display_memory_formatted(st,ed,wd);
            } else report_error("Invalid display target.",0);
        } else if (strcmp(token, "help") == 0) {
            printf("\nAvailable commands:\n"
                   "  load <address> [byte|word|dword] - Load data (default: byte)\n"
                   "  store <address> <value>        - Store byte to memory\n"
                   "  display cache                  - Show cache contents\n"
                   "  display memory <start> <end> [width] - Show memory (def width:16)\n"
                   "  display config                 - Show current configuration\n"
                   "  help                           - Show this help message\n"
                   "  quit | exit                    - Exit simulator\n");
        } else if (strcmp(token, "quit") == 0 || strcmp(token, "exit") == 0) {
            printf("Exiting simulator.\n"); break;
        } else {
            char unk_cmd_buf[100]; snprintf(unk_cmd_buf, sizeof(unk_cmd_buf), "Unknown command: %s", token);
            report_error(unk_cmd_buf, 0);
        }
    }

    // Cleanup
    if (cache != NULL) {
        if (NUMBER_OF_BLOCKS > 0 && BYTES_PER_BLOCK > 0) { 
             for (uint32_t i = 0; i < NUMBER_OF_BLOCKS; ++i) if (cache[i].data != NULL) free(cache[i].data);
        }
        free(cache);
    }
    if (memory != NULL) free(memory);
    printf("\nSimulator finished successfully.\n");
    return 0;
}
