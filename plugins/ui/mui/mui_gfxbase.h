#ifndef SELECTA_MUI_GFXBASE_H
#define SELECTA_MUI_GFXBASE_H

/* I719: the graphics.library base is renamed. libnix opens a library at program start when its base symbol (GfxBase)
 * is referenced, and the static MUI plugin is linked into selecta-cli too: there graphics.library is absent (vamos),
 * so the CLI exited 20 "graphics.library failed to load" before it ran anything (registry-machine-filter-vamos).
 * The plugin opens and closes graphics.library itself in mui_open / mui_close, the same pattern as the RTG backend
 * (plugins/ui/desktop/amiga/backend_rtg.c). Every MUI source that calls graphics.library includes this header instead of
 * <proto/graphics.h>. */
#define GfxBase sel_mui_GfxBase
#include <proto/graphics.h>

#endif
