#include <selecta/registry.h>

#include <stddef.h>

/* RP_SYSFILE parsing (I710ga1), compiled into the core so the engine (I710ga4) reads a replayer's declarations without
 * depending on the plugin registry. sel_registry_sysfiles wraps it with the registry's owner lookup and log. */
static int sysfile_name_ok(const char* n) {
    if (!n || !*n || n[0] == '.') {
        return 0;
    }
    for (; *n; n++) {
        char c = *n;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) {
            return 0;
        }
    }
    return 1;
}

/* ';'-separated list of 64 lowercase hex digests, at least one. */
static int sysfile_sha_ok(const char* s) {
    unsigned n = 0;
    if (!s) {
        return 0;
    }
    for (;; s++) {
        char c = *s;
        if (c == ';' || c == 0) {
            if (n != 64) {
                return 0;
            }
            n = 0;
            if (c == 0) {
                return 1;
            }
        } else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
            n++;
        } else {
            return 0;
        }
    }
}

unsigned sel_sysfile_decls(const sel_tag* hooks, const char* owner, sel_sysfile_decl* out, unsigned max,
                           void (*warn)(void* ctx, const char* plugin, const char* why), void* ctx) {
    const sel_tag* it = hooks;
    const sel_tag* t;
    unsigned n = 0;
    while (hooks && (t = sel_tag_next(&it)) != NULL) {
        const sel_tag* entry;
        sel_sysfile_decl d;
        const char* why = NULL;
        if (t->tag != SEL_RP_SYSFILE) {
            continue;
        }
        entry = (const sel_tag*)t->value;
        d.plugin = owner;
        d.name = entry ? (const char*)sel_tag_find(entry, SEL_RP_SYSFILE_NAME, 0) : NULL;
        d.label = entry ? (const char*)sel_tag_find(entry, SEL_RP_SYSFILE_LABEL, 0) : NULL;
        d.hint = entry ? (const char*)sel_tag_find(entry, SEL_RP_SYSFILE_HINT, (uintptr_t) "") : "";
        d.sha256 = entry ? (const char*)sel_tag_find(entry, SEL_RP_SYSFILE_SHA256, 0) : NULL;
        d.urls = entry ? (const char*)sel_tag_find(entry, SEL_RP_SYSFILE_URLS, (uintptr_t) "") : "";
        d.size = entry ? (uint32_t)sel_tag_find(entry, SEL_RP_SYSFILE_SIZE, 0) : 0;
        if (!entry || !d.name || !d.label || !d.sha256) {
            why = "SYSFILE entry missing NAME, LABEL or SHA256, ignored";
        } else if (!sysfile_name_ok(d.name)) {
            why = "SYSFILE NAME outside [A-Za-z0-9._-], ignored";
        } else if (!sysfile_sha_ok(d.sha256)) {
            why = "SYSFILE SHA256 is not 64 lowercase hex digits per entry, ignored";
        } else if (n >= max) {
            why = "too many SYSFILE entries, ignored";
        }
        if (why) {
            if (warn != NULL) {
                warn(ctx, owner, why);
            }
            continue;
        }
        out[n++] = d;
    }
    return n;
}
