/* The `aminet` source sync (row I405; online-sources plan section 5.1, D-OS-6): the module archives of Aminet
 * (https://aminet.net, the `mods/` hierarchy). The listing is one file, /INDEX (about 7 MB, 85,712 lines on
 * 2026-10-30): a few header lines that start with `|`, then one fixed-column line per package
 *
 *   AnarchyMenu10.lha              mods/4mat    5K 999 Chip mod by 4mat              ****+
 *   name (padded to 30)            dir          size age/downloads description
 *
 * The sync keeps the lines whose directory is `mods/` or below and whose file is an archive (`.lha`, `.lzh`, `.zip`);
 * every other data line (other directories, MP3/MPEG versions) is counted as skipped. A path is
 * `<dir>/<name>` (`mods/4mat/AnarchyMenu10.lha`), the song key is `aminet:<path>` and the URL is
 * https://aminet.net/<path>. The engine unpacks the first member some replayer takes when the song is opened. The
 * artist is the text after the last " by " of the description, else "- unknown"; the name is the file name without
 * its extension. */
#ifndef SELECTA_PLUGINS_SOURCES_AMINET_SYNC_H
#define SELECTA_PLUGINS_SOURCES_AMINET_SYNC_H

#include <selecta/host.h>
#include <selecta/source.h>

#include <stdint.h>

#define SEL_AMINET_LIST "INDEX"
#define SEL_AMINET_PATH_MAX 512u
#define SEL_AMINET_TEXT_MAX 192u
#define SEL_AMINET_FORMAT "Aminet module"

typedef struct sel_aminet_row {
    char path[SEL_AMINET_PATH_MAX];
    char artist[SEL_AMINET_TEXT_MAX];
    char name[SEL_AMINET_TEXT_MAX];
    uint32_t size; /* archive bytes, from the size column ("5K", "2.3M"), 0 when unreadable */
} sel_aminet_row;

typedef struct sel_aminet_sync_stats {
    uint32_t lines;        /* data lines read (header lines excluded) */
    uint32_t listing_rows; /* rows written to the catalogue */
    uint32_t skipped_rows; /* data lines dropped: outside mods/, not an archive, unsafe path, malformed */
    uint32_t parse_calls;  /* sel_aminet_parse_line calls: the sentinel that the parser ran */
} sel_aminet_sync_stats;

/* One INDEX line (no newline). Returns 1 and fills `row` when the line is a module archive of the mods/ hierarchy,
 * 0 for a header line, a line of another directory, another file type or a malformed line. */
int sel_aminet_parse_line(const char* line, sel_aminet_row* row);

/* Reads <dir>/INDEX into the catalogue. Returns 0 when the file cannot be read, memory runs out or the DB rejects a
 * batch. */
int sel_aminet_sync(const sel_host* host, const char* dir, sel_aminet_sync_stats* stats);

/* I1024g: reads one search answer `file` (the HTML table of https://aminet.net/search) and hands each package to `out`
 * with the sync's own row: every table row becomes an INDEX line and goes through sel_aminet_parse_line, so a hit and a
 * synced row share a key. The catalogue is not touched. Returns the rows given (0 for an answer without hits), -1 when
 * the answer cannot be read. */
int32_t sel_aminet_search_file(const sel_host* host, const char* file, const sel_search_sink* out);

#endif
