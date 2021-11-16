/* Unicorn Emulator Engine */
/* By Nguyen Anh Quynh, 2015 */

/* Sample code to demonstrate how to emulate ARM code */

#include <unicorn/unicorn.h>
#include <string.h>
// #include "uc_priv.h"


// code to be emulated
#define ARM_CODE "\x37\x00\xa0\xe3\x03\x10\x42\xe0" // mov r0, #0x37; sub r1, r2, r3
#define THUMB_CODE "\x83\xb0" // sub    sp, #0xc

#define ARM_THUM_COND_CODE "\x9a\x42\x14\xbf\x68\x22\x4d\x22" // 'cmp r2, r3\nit ne\nmov r2, #0x68\nmov r2, #0x4d'

/*
    push {r4, lr}  10b5
    mov.w r4, #0   4ff00004
    mov r0, r0     0046
    pop {r4, pc}   10bd
 */

// memory address where emulation starts

const int HOOK_MEM_BASE = 0x1000000;
const int HOOK_MEM_SIZE = 2 * 1024 * 1024;
const int ADDRESS = 0x1000000 + 4;
#define HOOK_MAGIC "\x00\x00\x00\x00"
#define TEST_CODE "\x10\xb5\x4f\xf0\x00\x04\x00\x46\x10\xbd"
#define TEST_CODE_1 "\x10\xb5\x4f\xf0\x00\x04\x00\x46\x10\xbd"

static void hook_block(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    printf(">>> Tracing basic block at 0x%"PRIx64 ", block size = 0x%x\n", address, size);
}

static void hook_code(uc_engine *uc, uint64_t address, uint32_t size, void *user_data)
{
    uint8_t bytes[4];
    char instructions[32] = {0};
    uc_mem_read(uc, address, bytes, size);
    for(int i = 0; i < size; i++){
        sprintf(instructions + i * 2, "%02x", bytes[i]);
    }
    printf(">>> Tracing instruction at 0x%"PRIx64 ", instruction size = 0x%x|%s\n", address, size, instructions);
    if (address == 0x1000006){
        int number;
        uc_reg_read(uc, UC_ARM_REG_R4, &number);
        printf("+++ Hook number:%d\n", number);
        struct uc_context* context;
        uc_context_alloc(uc, &context);
        uc_context_save(uc, context);
        printf("*** Nested call emu_start start ...\n");
        int err = uc_emu_start(uc, (HOOK_MEM_BASE + 16) | 1, (HOOK_MEM_BASE + 16) + 8, 0, 0);
        if (err) {
            printf("Failed on uc_emu_start() with error returned: %u\n", err);
        }
        printf("*** Nested call emu_start end\n");
        uc_context_restore(uc, context);
        uc_context_free(context);
        
    }
}


static void test_thumb(void)
{
    uc_engine *uc;
    uc_err err;
    uc_hook trace1;
    uc_hook trace2;

    int sp = HOOK_MEM_BASE + HOOK_MEM_SIZE;

    printf("Emulate THUMB code\n");

    // Initialize emulator in ARM mode
    err = uc_open(UC_ARCH_ARM, UC_MODE_THUMB, &uc);
    if (err) {
        printf("Failed on uc_open() with error returned: %u (%s)\n",
                err, uc_strerror(err));
        return;
    }

    // map 2MB memory for this emulation
    uc_mem_map(uc, HOOK_MEM_BASE, HOOK_MEM_BASE + HOOK_MEM_SIZE, UC_PROT_ALL);

    // write machine code to be emulated to memory
    printf("hook magic size: %lu\n", sizeof(HOOK_MAGIC));
    uc_mem_write(uc, HOOK_MEM_BASE, HOOK_MAGIC, sizeof(HOOK_MAGIC) - 1);
    uc_mem_write(uc, HOOK_MEM_BASE + 4, TEST_CODE, sizeof(TEST_CODE) - 1);
    uc_mem_write(uc, HOOK_MEM_BASE + 16, TEST_CODE, sizeof(TEST_CODE) - 1);

    // initialize machine registers
    uc_reg_write(uc, UC_ARM_REG_SP, &sp);

    // tracing all basic blocks with customized callback
    uc_hook_add(uc, &trace1, UC_HOOK_BLOCK, hook_block, NULL, 1, 0);

    // tracing one instruction at ADDRESS with customized callback
    int start_address = ADDRESS;
    uc_hook_add(uc, &trace2, UC_HOOK_CODE, hook_code, NULL, start_address, start_address + 0x1000);

    // emulate machine code in infinite time (last param = 0), or when
    // finishing all the code.
    // Note we start at ADDRESS | 1 to indicate THUMB mode.
    err = uc_emu_start(uc, ADDRESS | 1, ADDRESS + 8, 0, 0);
    if (err) {
        printf("Failed on uc_emu_start() with error returned: %u\n", err);
    }

    // now print out some registers
    printf(">>> Emulation done. Below is the CPU context\n");

    uc_reg_read(uc, UC_ARM_REG_SP, &sp);
    printf(">>> SP = 0x%x\n", sp);

    uc_close(uc);
}



int main(int argc, char **argv, char **envp)
{
    // dynamically load shared library
#ifdef DYNLOAD
    if (!uc_dyn_load(NULL, 0)) {
        printf("Error dynamically loading shared library.\n");
        printf("Please check that unicorn.dll/unicorn.so is available as well as\n");
        printf("any other dependent dll/so files.\n");
        printf("The easiest way is to place them in the same directory as this app.\n");
        return 1;
    }
#endif
    
    test_thumb();

    // dynamically free shared library
#ifdef DYNLOAD
    uc_dyn_free();
#endif
    
    return 0;
}
