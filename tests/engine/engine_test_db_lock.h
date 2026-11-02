/* I984/I986: SQLite is built SQLITE_THREADSAFE=0 (third_party/sqlite/CMakeLists.txt), so lib->db is safe only under call_lock, which every
 * library caller holds while the worker runs its request. An engine test that runs its own sqlite3_* statements on lib->db takes it too:
 * without it a posted read (lib_read_post, e.g. the folder counts after a sync) can run on the worker at the same time, corrupt the
 * connection's lookaside, and the next statement segfaults. Do not call an engine entry point between db_enter and db_leave: those take
 * call_lock themselves and the mutex is not recursive. */
#ifndef SELECTA_TESTS_ENGINE_TEST_DB_LOCK_H
#define SELECTA_TESTS_ENGINE_TEST_DB_LOCK_H

#include <pthread.h>

#include "db/db.h"
#include "db/db_internal.h"

static inline void db_enter(sel_library* lib) {
    pthread_mutex_lock(&lib->call_lock);
}

static inline void db_leave(sel_library* lib) {
    pthread_mutex_unlock(&lib->call_lock);
}

#endif
