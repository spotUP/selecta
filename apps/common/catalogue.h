/* An in-memory library seeded from a TSV catalogue (selecta-cli library search and
 * script --catalogue, selecta --catalogue). */
#ifndef SELECTA_APPS_CATALOGUE_H
#define SELECTA_APPS_CATALOGUE_H

struct sel_library;

/* Imports the TSV `tsv` (key, source, path, name, artist, format, year per line). NULL, with a
 * message on stderr, when it cannot. */
struct sel_library* sel_cli_open_catalogue(const char* tsv);
/* Same into the library DB file `db_path` (":memory:" for a throwaway DB); rows already there are upserted. */
struct sel_library* sel_cli_open_catalogue_at(const char* tsv, const char* db_path);

#endif
