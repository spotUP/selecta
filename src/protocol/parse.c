/* The protocol line parser shared by selecta-cli (script) and the selecta
 * shell (design doc section 3): one text line in, one command queued or one
 * ERROR event raised. Nothing here allocates. */
#include <selecta/engine.h>
#include <selecta/protocol.h>

int sel_engine_send_line(sel_engine* e, const char* line, char* pool, size_t poolcap) {
    sel_message m;
    char verb[64];
    int err = sel_codec_decode(line, SEL_MSG_CMD, &m, pool, poolcap);
    if (err == SEL_CODEC_OK) {
        if (!sel_engine_send(e, &m)) {
            sel_engine_report_error(e, "queue_full", NULL);
        }
        return SEL_CODEC_OK;
    }
    sel_codec_verb(line, verb, sizeof verb);
    sel_engine_report_error(e, err == SEL_CODEC_E_UNKNOWN_VERB ? "unknown_command" : sel_codec_strerror(err), verb);
    return err;
}
