/* Chromecast CASTV2 wire layer (I892e1, research row E): the 4-byte big-endian length frame, the hand-coded protobuf
 * CastMessage, and the JSON payloads the sender reads (RECEIVER_STATUS). Pure: no sockets, no TLS, so the Amiga build
 * compiles it without waiting for I899. The session over the socket table is cast_session.h. */
#ifndef SELECTA_SHARED_CAST_H
#define SELECTA_SHARED_CAST_H

#include <stddef.h>

#define SEL_CAST_NS_CONNECTION "urn:x-cast:com.google.cast.tp.connection"
#define SEL_CAST_NS_HEARTBEAT "urn:x-cast:com.google.cast.tp.heartbeat"
#define SEL_CAST_NS_RECEIVER "urn:x-cast:com.google.cast.receiver"
#define SEL_CAST_NS_MEDIA "urn:x-cast:com.google.cast.media"

#define SEL_CAST_ID_MAX 64
#define SEL_CAST_NS_MAX 96
#define SEL_CAST_PAYLOAD_MAX 4096u
#define SEL_CAST_FRAME_MAX (SEL_CAST_PAYLOAD_MAX + 512u) /* body bytes after the length prefix */

typedef struct sel_cast_msg {
    char source[SEL_CAST_ID_MAX];
    char dest[SEL_CAST_ID_MAX];
    char ns[SEL_CAST_NS_MAX];
    char payload[SEL_CAST_PAYLOAD_MAX]; /* payload_utf8; empty for a BINARY message */
    int binary;                         /* payload_type was BINARY */
} sel_cast_msg;

/* Writes length prefix + CastMessage (protocol_version CASTV2_1_0, payload_type STRING) into out. Returns the total
 * byte count, or 0 when it does not fit in cap or a field is too long. */
size_t sel_cast_encode(unsigned char* out, size_t cap, const char* source, const char* dest, const char* ns,
                       const char* payload_utf8);

/* Decodes a CastMessage body (without the length prefix). Unknown fields are skipped. 0 on success, -1 malformed or
 * a field longer than the struct holds. */
int sel_cast_decode(const unsigned char* body, size_t len, sel_cast_msg* out);

/* Reads the 4-byte big-endian frame length. */
unsigned long sel_cast_frame_len(const unsigned char hdr[4]);

typedef struct sel_cast_status {
    char type[32];           /* "RECEIVER_STATUS", "PING", "PONG", "CLOSE", ... */
    long request_id;         /* -1 when absent */
    int app_count;           /* applications listed */
    char app_id[32];         /* first application */
    char transport_id[SEL_CAST_ID_MAX];
    char session_id[SEL_CAST_ID_MAX];
    char display_name[64];
    int has_volume;
    double volume_level;     /* 0..1 */
    int volume_muted;
    /* MEDIA_STATUS (I892e2): the first entry of the "status" array */
    int media_count;         /* entries in the status array */
    long media_session_id;   /* mediaSessionId of the first entry, 0 when absent */
    char player_state[24];   /* "IDLE", "BUFFERING", "PLAYING", "PAUSED" */
    char idle_reason[24];    /* "FINISHED", "ERROR", ... when IDLE */
    double current_time;     /* seconds */
    double duration;         /* seconds, 0 when unknown */
} sel_cast_status;

/* Parses a JSON payload: the top-level "type" and requestId, and for RECEIVER_STATUS the first application and the
 * volume, and for MEDIA_STATUS the first media entry (state, currentTime, duration). Reads through json_scan (plugins/shared/source). 0 on success, -1 on malformed JSON. */
int sel_cast_parse_status(const char* json, sel_cast_status* out);

#endif
