/* Which file names the registered replayers claim (I218i): the folder browser lists a file only when
 * a replayer the machine can run declares its extension (SEL_RP_EXTENSIONS, comma list). The registry
 * is the single source of truth; nothing is cached, so a plugin the machine filtered out never claims. */
#ifndef SELECTA_ENGINE_CLAIMS_H
#define SELECTA_ENGINE_CLAIMS_H

#include <selecta/tags.h>

#include <stdint.h>

/* 1 when some plugin from `registry_get(0..registry_count()-1)` lists, in SEL_RP_EXTENSIONS, the
 * last extension of `name` ("x.mod") or its Amiga prefix, the part before the first dot ("mod.x"),
 * case-insensitively. 0 for a NULL registry, a name without a dot, or an empty extension. The engine
 * core does not link the registry, so this takes the same count/get pair the engine is configured with. */
int sel_registry_claims_name(uint32_t (*registry_count)(void), const sel_tag* (*registry_get)(uint32_t), const char* name);

#endif
