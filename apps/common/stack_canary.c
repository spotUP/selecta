#include "stack_canary.h"

#include <string.h>

void sel_stack_canary_fill(unsigned char* low, unsigned long bytes) {
    memset(low, SEL_STACK_FILL_BYTE, bytes);
    memset(low, SEL_STACK_CANARY_BYTE, SEL_STACK_CANARY);
}

int sel_stack_canary_ok(const unsigned char* low) {
    unsigned i;
    for (i = SEL_STACK_EXEC_COOKIE; i < SEL_STACK_CANARY; i++) {
        if (low[i] != SEL_STACK_CANARY_BYTE) {
            return 0;
        }
    }
    return 1;
}

unsigned long sel_stack_untouched(const unsigned char* low, unsigned long bytes) {
    unsigned long i = SEL_STACK_CANARY;
    while (i < bytes && low[i] == SEL_STACK_FILL_BYTE) {
        i++;
    }
    if (i == SEL_STACK_CANARY && i < bytes && !sel_stack_canary_ok(low)) {
        return 0; /* the stack ran down through the fill into the canary */
    }
    return i;
}
