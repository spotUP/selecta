/* I685e4: sel_mcc_static links without the MUI plugin archive; the baked Lucide table is part of the package. */
#include "mui_icons.h"
#include <stdio.h>

int main(void) {
    int id = mui_icon_id("play");
    const uint8_t* body;
    if (id < 0) {
        fprintf(stderr, "[ERROR] icon play not found in the baked table\n");
        return 1;
    }
    body = mui_icon_body_wh(id, 24, 24);
    if (!body) {
        fprintf(stderr, "[ERROR] no 24x24 body for play\n");
        return 1;
    }
    return 0;
}
