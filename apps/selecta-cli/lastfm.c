/* `selecta-cli lastfm login [--config-dir <dir>]` (I177b): the last.fm desktop auth flow through the scrobbler
 * plugin's INT_LOGIN hook. It asks for a request token, prints the page where the person authorises it, waits
 * for one line on stdin (Enter, or end of input), asks for the session key, and stores the API key, the shared
 * secret and the session key as engine prefs, so they land in <config>/prefs.conf and the plugin reads them
 * from there on the next start (sel_engine_plugin_host). The key and secret come from the prefs or from
 * SELECTA_LASTFM_KEY / SELECTA_LASTFM_SECRET. */
#include "cli.h"
#include "integration_host.h"

#include <selecta/engine.h>
#include <selecta/machine.h>
#include <selecta/protocol.h>
#include <selecta/registry.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_event(void* ctx, const sel_message* evt) {
    if (evt->id == SEL_EVT_ERROR) {
        *(int*)ctx = 1;
    }
}

/* The pref when it is set, else the environment variable; "" when neither. */
static const char* setting(sel_engine* e, const char* pref, const char* env) {
    const sel_host* h = sel_engine_plugin_host(e);
    const char* v = h->config_get != NULL ? h->config_get(h, pref, NULL) : NULL;
    if (v == NULL || v[0] == '\0') v = getenv(env);
    return v != NULL ? v : "";
}

static int store(sel_engine* e, const char* pref, const char* value) {
    char line[300];
    char pool[1024];
    int failed = 0;
    snprintf(line, sizeof line, "PREF_SET %s %s", pref, value);
    if (sel_engine_send_line(e, line, pool, sizeof pool) != SEL_CODEC_OK) return 0;
    sel_engine_pump(e, on_event, &failed);
    return !failed;
}

static int login(sel_engine* e) {
    char out[512];
    char token[160];
    char key[96];
    char secret[96];
    char line[64];
    const char* v;
    const char* user;
    int rc = sel_int_login(e, "lastfm", NULL, out, sizeof out);

    if (rc < 0) {
        fprintf(stderr, "selecta-cli: the last.fm plugin is not available\n");
        return 1;
    }
    if (rc != 0 || strncmp(out, "token=", 6) != 0) {
        printf("lastfm_login=failed reason=%s\n", out);
        return 1;
    }
    snprintf(token, sizeof token, "%.*s", (int)strcspn(out + 6, " "), out + 6);
    v = strstr(out, " url=");
    printf("lastfm_auth_url=%s\n", v != NULL ? v + 5 : "");
    fprintf(stderr, "Open the page above, allow Selecta, then press Enter.\n");
    fflush(stdout);
    if (fgets(line, sizeof line, stdin) == NULL) line[0] = '\0';

    rc = sel_int_login(e, "lastfm", token, out, sizeof out);
    if (rc != 0 || strncmp(out, "session=", 8) != 0) {
        printf("lastfm_login=failed reason=%s\n", out);
        return 1;
    }
    user = strstr(out, " user=");
    out[strcspn(out, " ")] = '\0';
    snprintf(key, sizeof key, "%s", setting(e, "scrobble.lastfm_key", "SELECTA_LASTFM_KEY"));
    snprintf(secret, sizeof secret, "%s", setting(e, "scrobble.lastfm_secret", "SELECTA_LASTFM_SECRET"));
    if (!store(e, "scrobble.lastfm_key", key) || !store(e, "scrobble.lastfm_secret", secret) ||
        !store(e, "scrobble.lastfm_session", out + 8)) {
        printf("lastfm_login=failed reason=prefs_save_failed\n");
        return 1;
    }
    printf("lastfm_login=ok user=%s\n", user != NULL ? user + 6 : "");
    return 0;
}

int sel_cli_lastfm(int argc, char** argv) {
    sel_engine_config cfg;
    sel_engine* e;
    char dir[256];
    const char* config = NULL;
    int rc;
    int i;

    if (argc < 1 || strcmp(argv[0], "login") != 0) {
        fprintf(stderr, "usage: selecta-cli lastfm login [--config-dir <dir>]\n");
        return 2;
    }
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config-dir") == 0 && i + 1 < argc) {
            config = argv[++i];
        } else {
            fprintf(stderr, "selecta-cli: lastfm login takes no argument %s\n", argv[i]);
            return 2;
        }
    }
    if (config == NULL) {
        if (!sel_config_dir(dir, sizeof dir)) {
            fprintf(stderr, "selecta-cli: no config directory\n");
            return 1;
        }
        config = dir;
    }
    (void)sel_config_mkdir(config);
    memset(&cfg, 0, sizeof cfg);
    sel_registry_init(sel_host_default());
    cfg.registry_count = sel_registry_count;
    cfg.registry_get = sel_registry_get;
    cfg.machine = sel_machine_probe(sel_host_default());
    cfg.config_dir = config;
    e = sel_engine_create(sel_host_default(), &cfg);
    if (e == NULL) {
        fputs("selecta-cli: engine allocation failed\n", stderr);
        return 1;
    }
    rc = login(e);
    sel_engine_destroy(e);
    return rc;
}
