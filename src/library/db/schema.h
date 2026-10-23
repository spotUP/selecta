/* Schema v1 and migrations (plan Phase 1, I146). */
#ifndef SELECTA_LIBRARY_SCHEMA_H
#define SELECTA_LIBRARY_SCHEMA_H

#include <sqlite3.h>

/* PRAGMA user_version, or -1 on error. */
int sel_schema_version(sqlite3* db);
/* Applies every missing migration; 1 on success, 0 on failure or a DB newer than this build. */
int sel_schema_migrate(sqlite3* db);

#endif
