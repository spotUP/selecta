/* An in-memory library seeded from a TSV catalogue (selecta-cli library search and
 * script --catalogue, selecta --catalogue). */
#ifndef SELECTA_APPS_CATALOGUE_H
#define SELECTA_APPS_CATALOGUE_H

struct sel_library;

/* Imports the TSV `tsv` (key, source, path, name, artist, format, year per line). NULL, with a
 * message on stderr, when it cannot. */
struct sel_library* sel_cli_open_catalogue(const char* tsv);

#endif
