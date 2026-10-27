/* MUI plugin mapping functions (plan 2026-10-25-amiga-mui.md section 2.2). Pure C: no <proto/muimaster.h>, so the
 * host builds and tests this directory (MUI-P1-1). */
#ifndef SEL_MUI_MAP_H
#define SEL_MUI_MAP_H

#include <stdint.h>

/* The view styles the MUI UI offers. An RTG screen can host the custom canvas (grid, cover flow), so every style;
 * a planar screen has only native list widgets, so the classic list styles (plan section 1, finish line 3). */
uint32_t mui_style_mask(int screen_is_rtg);

/* MUI-P1-7: the SEL_UI_WAIT contract as a loop over two callbacks, so the host tests it and the Amiga build runs the
 * same code. new_input is MUIM_Application_NewInput (returns the ReturnID, MUI_WAIT_ID_QUIT for the quit id, and the
 * signals MUI wants waited on); wait blocks up to timeout_ms on a signal mask and returns the signals that arrived. */
#define MUI_WAIT_ID_QUIT (-1)
typedef struct mui_wait_io {
    int (*new_input)(void* ctx, uint32_t* sigs);
    uint32_t (*wait)(void* ctx, uint32_t mask, uint32_t timeout_ms);
    uint32_t break_mask; /* Ctrl-C: ends the UI like the close gadget */
    uint32_t wake_mask;  /* the engine wake signal: part of every wait mask, counts as handled */
} mui_wait_io;
/* 0 timeout, 1 input handled, -1 quit. */
int mui_wait_step(const mui_wait_io* io, void* ctx, uint32_t timeout_ms);

/* MUI-P1-6: is MUI 3.8 (muimaster.library v19) usable. The probe is the library open on the Amiga and "absent" on the
 * host until a test installs a fake. The plugin entry returns no descriptor when it says no, so the registry never
 * lists `mui` and `--ui mui` is refused as unregistered. */
typedef int (*mui_probe_fn)(void);
void mui_set_probe(mui_probe_fn probe);
int mui_present(void);

/* MUI-P1-4: the distinct failure texts printed and shown in the EasyRequest when the UI cannot open. */
typedef enum mui_fail { MUI_FAIL_NO_LIBRARY = 0, MUI_FAIL_NO_WINDOW = 1 } mui_fail;
const char* mui_fail_text(mui_fail why);

#endif
