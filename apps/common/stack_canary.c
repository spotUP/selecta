#include "stack_canary.h"

#include <string.h>

void sel_stack_canary_fill(unsigned char* low, unsigned long bytes) {
    memset(low, SEL_STACK_FILL_BYTE, bytes);
    memset(low, SEL_STACK_CANARY_BYTE, SEL_STACK_CANARY);
}

int sel_stack_canary_ok(const unsigned char* low) {
    unsigned i;
    for (i = 0; i < SEL_STACK_CANARY; i++) {
        if (low[i] != SEL_STACK_CANARY_BYTE) {
            return 0;
        }
    }
    return 1;
}

unsigned long sel_stack_untouched(const unsigned char* low, unsigned long bytes) {
    unsigned long i = SEL_STACK_CANARY;
    if (!sel_stack_canary_ok(low)) {
        return 0;
    }
    while (i < bytes && low[i] == SEL_STACK_FILL_BYTE) {
        i++;
    }
    return i;
}
