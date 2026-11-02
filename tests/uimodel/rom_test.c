/* MPS-1c: the C64 ROM service. Exact sizes (8192 / 8192 / 4096), a refused file copies nothing, the source follows the file in
 * the config dir, and reset removes it. */
#include <selecta/uimodel.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int g_fail;

#define CHECK(c)                                        \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL line %d: %s\n", __LINE__, #c); \
            g_fail++;                                   \
        }                                               \
    } while (0)

static void put(const char* path, size_t n) {
    static unsigned char buf[16384];
    FILE* f = fopen(path, "wb");
    memset(buf, 0x5a, sizeof buf);
    if (f != NULL) {
        fwrite(buf, 1, n, f);
        fclose(f);
    }
}

int main(int argc, char** argv) {
    char dir[512], src[600], dst[600], err[96];
    uint32_t slot;
    static const size_t good[3] = {8192, 8192, 4096};
    if (argc < 2) {
        return 2;
    }
    snprintf(dir, sizeof dir, "%s", argv[1]);
    mkdir(dir, 0755);
    snprintf(src, sizeof src, "%s/src.bin", dir);
    CHECK(sel_uim_rom_slot("kernal") == 0 && sel_uim_rom_slot("basic") == 1 && sel_uim_rom_slot("chargen") == 2 && sel_uim_rom_slot("x") == -1);
    for (slot = 0; slot < SEL_UIM_ROM_COUNT; slot++) {
        static const char* const names[3] = {"kernal", "basic", "chargen"};
        size_t bad[3] = {good[slot] - 1u, good[slot] + 1u, 0u};
        int k;
        snprintf(dst, sizeof dst, "%s/%s", dir, names[slot]);
        CHECK(sel_uim_rom_size(slot) == good[slot]);
        CHECK(!sel_uim_rom_is_user(dir, slot));
        for (k = 0; k < 3; k++) {
            put(src, bad[k]);
            err[0] = '\0';
            CHECK(!sel_uim_rom_install(dir, slot, src, err, sizeof err));
            CHECK(strstr(err, "Wrong size") != NULL);
            CHECK(!sel_uim_rom_is_user(dir, slot));
        }
        CHECK(!sel_uim_rom_install(dir, slot, "/nonexistent/rom", err, sizeof err) && err[0] != '\0');
        put(src, good[slot]);
        CHECK(sel_uim_rom_install(dir, slot, src, err, sizeof err) && err[0] == '\0');
        CHECK(sel_uim_rom_is_user(dir, slot));
        CHECK(sel_uim_rom_install(dir, slot, src, err, sizeof err)); /* replaces the earlier copy */
        CHECK(strcmp(sel_uim_rom_source_text(sel_uim_rom_is_user(dir, slot)), "Your ROM") == 0);
        CHECK(sel_uim_rom_reset(dir, slot) && !sel_uim_rom_is_user(dir, slot));
        CHECK(strcmp(sel_uim_rom_source_text(sel_uim_rom_is_user(dir, slot)), "Open ROMs (bundled)") == 0);
    }
    remove(src);
    if (g_fail == 0) {
        printf("rom: ok\n");
    }
    return g_fail != 0;
}
