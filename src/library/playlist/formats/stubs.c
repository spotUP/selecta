/* Temporary red-phase stubs (removed in the green commit). */
#include "../playlist_internal.h"

int sel_pl_read_m3u(const char* b, size_t l, sel_playlist* pl) { (void)b; (void)l; (void)pl; return SEL_PL_FORMAT; }
int sel_pl_read_pls(const char* b, size_t l, sel_playlist* pl) { (void)b; (void)l; (void)pl; return SEL_PL_FORMAT; }
int sel_pl_read_xspf(const char* b, size_t l, sel_playlist* pl) { (void)b; (void)l; (void)pl; return SEL_PL_FORMAT; }
int sel_pl_write_m3u(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn w, void* c) { (void)pl; (void)o; (void)w; (void)c; return SEL_PL_FORMAT; }
int sel_pl_write_pls(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn w, void* c) { (void)pl; (void)o; (void)w; (void)c; return SEL_PL_FORMAT; }
int sel_pl_write_xspf(const sel_playlist* pl, sel_pl_out* o, sel_pl_warn_fn w, void* c) { (void)pl; (void)o; (void)w; (void)c; return SEL_PL_FORMAT; }
