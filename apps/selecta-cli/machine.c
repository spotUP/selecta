/* `selecta-cli machine`: runs the probe once and prints the sentinel
 * counter, then one NAME=value line per SEL_MACHINE_* tag. */
#include <selecta/machine.h>

#include "cli.h"

#include <stdio.h>
#include <string.h>

#define MACHINE_PREFIX "MACHINE_"

int sel_cli_machine(void) {
    const sel_tag* list = sel_machine_probe(sel_host_default());
    const sel_tag* iter = list;
    const sel_tag* t;

    printf("probe_calls=%u\n", (unsigned)sel_machine_probe_calls());
    while ((t = sel_tag_next(&iter)) != NULL) {
        const char* name = sel_tag_name(t->tag);
        sel_tag_type type = SEL_TT_INT;

        if (name == NULL || strncmp(name, MACHINE_PREFIX, sizeof(MACHINE_PREFIX) - 1) != 0) {
            continue;
        }
        name += sizeof(MACHINE_PREFIX) - 1;
        (void)sel_tag_type_of(t->tag, &type);
        if (t->tag == SEL_MACHINE_CPU) {
            printf("%s=%s\n", name, sel_machine_cpu_name((uint32_t)t->value));
        } else if (t->tag == SEL_MACHINE_TIER) {
            printf("%s=%s\n", name, sel_machine_tier_name((uint32_t)t->value));
        } else if (type == SEL_TT_STR) {
            printf("%s=%s\n", name, (const char*)t->value);
        } else {
            printf("%s=%lu\n", name, (unsigned long)t->value);
        }
    }
    return 0;
}
