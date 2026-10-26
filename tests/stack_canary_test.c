/* I376: the bottom-of-stack canary and high-water mark (apps/common/stack_canary.c). Pure functions. */
#include "stack_canary.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                \
            failures++;                                                   \
        }                                                                 \
    } while (0)

#define STACK 4096u
static unsigned char stack[STACK];

int main(void) {
    sel_stack_canary_fill(stack, STACK);
    CHECK(sel_stack_canary_ok(stack));
    CHECK(sel_stack_untouched(stack, STACK) == STACK); /* nothing used yet */

    /* A body that used the top 1000 bytes: canary intact, untouched = the rest. */
    memset(stack + STACK - 1000u, 0x11, 1000u);
    CHECK(sel_stack_canary_ok(stack));
    CHECK(sel_stack_untouched(stack, STACK) == STACK - 1000u);

    /* One byte overwritten at the very bottom, and at the top edge of the canary: broken. */
    stack[0] ^= 0xFFu;
    CHECK(!sel_stack_canary_ok(stack));
    CHECK(sel_stack_untouched(stack, STACK) == 0u);
    sel_stack_canary_fill(stack, STACK);
    stack[SEL_STACK_CANARY - 1u] = 0u;
    CHECK(!sel_stack_canary_ok(stack));

    /* A write just above the canary leaves it intact and shows in the high-water mark. */
    sel_stack_canary_fill(stack, STACK);
    stack[SEL_STACK_CANARY + 10u] = 0u;
    CHECK(sel_stack_canary_ok(stack));
    CHECK(sel_stack_untouched(stack, STACK) == SEL_STACK_CANARY + 10u);

    if (failures == 0) {
        printf("stack_canary_unit_ok\n");
        return 0;
    }
    return 1;
}
