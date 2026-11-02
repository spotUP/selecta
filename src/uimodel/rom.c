/* MPS-1c: the user's own C64 ROMs. Settings copies a KERNAL, BASIC or character ROM into the config directory under the
 * names the sid adapter reads (kernal, basic, chargen); an absent or wrong-sized file means the bundled Open ROMs play.
 * Plain C99: the size check, the copy, the source query and the reset live here, the Settings page only calls them. */
#include <selecta/uimodel.h>
#include <stdio.h>
#include <string.h>

static const char* const g_names[SEL_UIM_ROM_COUNT] = {"kernal", "basic", "chargen"};
static const uint32_t g_sizes[SEL_UIM_ROM_COUNT] = {8192u, 8192u, 4096u};

int sel_uim_rom_slot(const char* name) {
    uint32_t i;
    for (i = 0; name != NULL && i < SEL_UIM_ROM_COUNT; i++) {
        if (strcmp(name, g_names[i]) == 0) {
            return (int)i;
        }
    }
    return -1;
}

uint32_t sel_uim_rom_size(uint32_t slot) { return slot < SEL_UIM_ROM_COUNT ? g_sizes[slot] : 0u; }

static int rom_path(const char* dir, uint32_t slot, const char* suffix, char* out, size_t cap) {
    if (dir == NULL || dir[0] == '\0' || slot >= SEL_UIM_ROM_COUNT) {
        return 0;
    }
    return snprintf(out, cap, "%s/%s%s", dir, g_names[slot], suffix) < (int)cap;
}

/* Byte count of a file, -1 when it cannot be read. */
static long file_size(const char* path) {
    long n;
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return -1;
    }
    n = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
    fclose(f);
    return n;
}

static void put_err(char* err, size_t cap, const char* text) {
    if (err != NULL && cap > 0u) {
        snprintf(err, cap, "%s", text);
    }
}

int sel_uim_rom_install(const char* config_dir, uint32_t slot, const char* src, char* err, size_t cap) {
    char dst[600], tmp[608], msg[96];
    unsigned char buf[8192];
    long n;
    FILE *in, *out;
    if (!rom_path(config_dir, slot, "", dst, sizeof dst) || !rom_path(config_dir, slot, ".tmp", tmp, sizeof tmp) || src == NULL) {
        put_err(err, cap, "No configuration folder.");
        return 0;
    }
    n = file_size(src);
    if (n < 0) {
        put_err(err, cap, "The file could not be read.");
        return 0;
    }
    if (n != (long)g_sizes[slot]) {
        snprintf(msg, sizeof msg, "Wrong size: %ld bytes, expected %u.", n, (unsigned)g_sizes[slot]);
        put_err(err, cap, msg);
        return 0;
    }
    in = fopen(src, "rb");
    out = in != NULL ? fopen(tmp, "wb") : NULL;
    if (in == NULL || out == NULL || fread(buf, 1, g_sizes[slot], in) != g_sizes[slot] || fwrite(buf, 1, g_sizes[slot], out) != g_sizes[slot]) {
        if (in != NULL) {
            fclose(in);
        }
        if (out != NULL) {
            fclose(out);
            remove(tmp);
        }
        put_err(err, cap, "The ROM could not be copied.");
        return 0;
    }
    fclose(in);
    if (fclose(out) != 0) {
        remove(tmp);
        put_err(err, cap, "The ROM could not be copied.");
        return 0;
    }
    remove(dst); /* rename() does not replace an existing file on every platform */
    if (rename(tmp, dst) != 0) {
        remove(tmp);
        put_err(err, cap, "The ROM could not be copied.");
        return 0;
    }
    put_err(err, cap, "");
    return 1;
}

int sel_uim_rom_is_user(const char* config_dir, uint32_t slot) {
    char path[600];
    return rom_path(config_dir, slot, "", path, sizeof path) && file_size(path) == (long)g_sizes[slot];
}

const char* sel_uim_rom_source_text(int user) { return user ? "Your ROM" : "Open ROMs (bundled)"; }

int sel_uim_rom_reset(const char* config_dir, uint32_t slot) {
    char path[600];
    if (!rom_path(config_dir, slot, "", path, sizeof path)) {
        return 0;
    }
    remove(path);
    return file_size(path) < 0;
}
