/* Where Selecta keeps prefs.conf, keys.conf and history (plan M1.2, "Files
 * and locations"). One rule for macOS and Linux: $XDG_CONFIG_HOME/selecta,
 * else $HOME/.config/selecta. Amiga: ENVARC:Selecta. Pure string work; the
 * directory is not created here. */
#include <selecta/engine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int sel_config_dir(char* out, size_t cap) {
#if defined(__amigaos__) || defined(AMIGA)
    int n = snprintf(out, cap, "ENVARC:Selecta");
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
