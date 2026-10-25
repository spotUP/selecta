/* Where Selecta keeps prefs.conf, keys.conf and history (plan M1.2, "Files
 * and locations"). One rule for macOS and Linux: $XDG_CONFIG_HOME/selecta,
 * else $HOME/.config/selecta. Amiga (I214; plan section "Files and locations"):
 * ENVARC:Selecta when the ENVARC: assign exists, else PROGDIR:Selecta (where the
 * program was started from). The assign is probed with DOS requesters off
 * (src/core/amiga/dos_quiet.c); an unassigned ENVARC: used to open an invisible
 * "insert volume" requester and block the program. The directory is not
 * created here. */
#include <selecta/engine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__amigaos__) || defined(AMIGA)
int sel_amiga_name_exists(const char* name); /* src/core/amiga/dos_quiet.c */
#endif

int sel_config_dir(char* out, size_t cap) {
#if defined(__amigaos__) || defined(AMIGA)
    int n = snprintf(out, cap, "%s", sel_amiga_name_exists("ENVARC:") ? "ENVARC:Selecta" : "PROGDIR:Selecta");
#else
    const char* xdg = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    int n;
    if (xdg != NULL && xdg[0] != '\0') {
        n = snprintf(out, cap, "%s/selecta", xdg);
    } else if (home != NULL && home[0] != '\0') {
        n = snprintf(out, cap, "%s/.config/selecta", home);
    } else {
        return 0;
    }
#endif
    return n > 0 && (size_t)n < cap;
}

/* The default library DB (I276): PROGDIR:library.db on the Amiga, else library.db in the config directory, which is
 * created when missing. selecta and selecta-cli share this one rule. */
const char* sel_default_db_path(char* out, size_t cap) {
#if defined(__amigaos__) || defined(AMIGA)
    snprintf(out, cap, "PROGDIR:library.db");
    return out;
#else
    char dir[256];
    if (!sel_config_dir(dir, sizeof dir) || !sel_config_mkdir(dir)) {
        return NULL;
    }
    return snprintf(out, cap, "%s/library.db", dir) < (int)cap ? out : NULL;
#endif
}
