/* Playlist folder tree operations of the library worker (I178, design doc 9c, plan T83). Called from
 * sel_library_user for the SEL_USER_FOLDER_* and SEL_USER_PLAYLIST_* ops; private to src/library/db/. */
#ifndef SELECTA_LIBRARY_DB_FOLDERS_H
#define SELECTA_LIBRARY_DB_FOLDERS_H

#include "db.h"

#include <sqlite3.h>

/* Runs one folder/playlist op. Returns SQLITE_OK or SQLITE_CONSTRAINT (a rule refused it; *rows = a
 * SEL_FOLDER_ERR_* code). On success *rows is the op's result (new id, rows written) and counts[] is filled
 * for SEL_USER_FOLDER_COUNTS. */
int sel_folder_exec(sqlite3* db, const sel_user_op* op, int64_t* rows, int32_t counts[4]);

#endif
