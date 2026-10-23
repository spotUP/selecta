/* Folder export of the playlist tree (I178, design doc 9c): `selecta-cli library export-folder`. Kept out of
 * library.c because that file sees the song-source sel_playlist type and this one the playlist codec's. */
#include <selecta/machine.h>
#include <selecta/registry.h>
#include "cli.h"
#include "db.h"
#include "playlist/playlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int export_sink(void* ctx, const char* bytes, uint32_t len) {
    return fwrite(bytes, 1, len, (FILE*)ctx) == len;
}

/* I178: writes every playlist under `folder` (0 = all, sub-folders included) as one file per playlist into `dir`,
 * named <playlist id>-<name>.<ext>; returns the number of files written, or -1 on failure. */
static int folder_export(sel_library* lib, uint32_t folder, const char* dir, const char* ext) {
    char list[8192];
    char keys[16384];
    char probe[32];
    sel_pl_format fmt;
    const char* p = list;
    uint32_t count = 0, written = 0;
    sel_pl_entry* entries = malloc(sizeof(sel_pl_entry) * 512u);
    char* text = malloc(65536u);
    int fail = entries == NULL || text == NULL;

    snprintf(probe, sizeof probe, "x.%s", ext);
    fmt = sel_playlist_format_from_name(probe);
    if (fmt == SEL_PL_AUTO) fail = 1;
    if (!fail) mkdir(dir, 0777);
    if (!fail && sel_library_folder_playlists(lib, folder, list, sizeof list, &count) != SEL_FOLDER_OK) fail = 1;
    while (!fail && *p != '\0') {
        const char* name = p + strlen(p) + 1;
        uint32_t id = (uint32_t)strtoul(p, NULL, 10), nkeys = 0;
        char path[512], clean[96];
        const char* k;
        sel_playlist pl;
        FILE* f;
        size_t i, n = 0;

        p = name + strlen(name) + 1;
        for (i = 0; name[i] != '\0' && n + 1 < sizeof clean; i++) {
            char c = name[i];
            clean[n++] = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' ? c : '_';
        }
        clean[n] = '\0';
        if (sel_library_playlist_keys(lib, id, keys, sizeof keys, &nkeys) != SEL_FOLDER_OK) {
            fail = 1;
            break;
        }
        sel_playlist_init(&pl, entries, 512u, text, 65536u);
        for (k = keys; *k != '\0'; k += strlen(k) + 1) {
            if (sel_playlist_add(&pl, SEL_PL_SONG, k, strlen(k), NULL, 0, 0, 0) != 0) {
                fail = 1;
                break;
            }
        }
        snprintf(path, sizeof path, "%s/%u-%s.%s", dir, (unsigned)id, clean, ext);
        f = fail ? NULL : fopen(path, "wb");
        if (f == NULL) {
            fail = 1;
            break;
        }
        if (sel_playlist_write(fmt, &pl, export_sink, f, NULL, NULL) != SEL_PL_OK) fail = 1;
        if (fclose(f) != 0) fail = 1;
        written++;
    }
    free(entries);
    free(text);
    return fail ? -1 : (int)written;
}

/* library export-folder --db <path> --dir <dir> [--folder <id>] [--format m3u|m3u8|pls|xspf|prg] */
int sel_cli_library_export_folder(int argc, char** argv) {
    const sel_host* host = sel_host_default();
    const char* db_path = NULL;
    const char* dir = NULL;
    const char* ext = "m3u";
    uint32_t folder = 0;
    sel_library* lib;
    int i, n;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
            dir = argv[++i];
        } else if (strcmp(argv[i], "--folder") == 0 && i + 1 < argc) {
            folder = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
            ext = argv[++i];
        } else {
            db_path = NULL;
            break;
        }
    }
    if (db_path == NULL || dir == NULL) {
        fputs("usage: selecta-cli library export-folder --db <path> --dir <dir> [--folder <id>] [--format m3u|m3u8|pls|xspf|prg]\n", stderr);
        return 2;
    }
    sel_registry_init(host);
    lib = sel_library_open(host, sel_machine_probe(host), 0, db_path);
    if (lib == NULL) {
        puts("db_open=0");
        return 1;
    }
    n = folder_export(lib, folder, dir, ext);
    printf("exported=%d folder=%u\n", n, (unsigned)folder);
    sel_library_close(lib);
    return n < 0 ? 1 : 0;
}

