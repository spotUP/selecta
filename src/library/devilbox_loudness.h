/* DEViLBOX loudness endpoint client (I188, D-M3-9, D-M3-13, design doc 6a): GET/PUT <devilbox>/api/loudness/<md5>
 * through the one HTTP client. Integer arithmetic only, so the 68k build can fetch a gain without a float unit.
 * The hash is the lower-case md5 of the file bytes (md5.h). */
#ifndef SELECTA_LIBRARY_DEVILBOX_LOUDNESS_H
#define SELECTA_LIBRARY_DEVILBOX_LOUDNESS_H

#include <stddef.h>
#include <stdint.h>

#define SEL_DVB_LOUDNESS_TARGET_CDB (-1800)  /* -18 LUFS, same target as the analyser (D-M3-9) */
#define SEL_DVB_LOUDNESS_GAIN_LIMIT_CDB 2400 /* +-24 dB, same limit as the analyser */

typedef enum sel_dvb_loudness_status {
    SEL_DVB_LOUDNESS_HIT = 1,     /* 200: answer parsed */
    SEL_DVB_LOUDNESS_UNKNOWN = 0, /* 404 {"error":"unknown hash"}: the endpoint is there, nobody analysed this file */
    SEL_DVB_LOUDNESS_MISSING = -1 /* route missing, network down, 429/5xx, bad answer: degrade to local only (D-M3-13) */
} sel_dvb_loudness_status;

typedef struct sel_dvb_loudness {
    int32_t lufs_cdb; /* centi-LUFS */
    int32_t peak_cdb; /* centi-dBFS */
    int r128;         /* method "r128" (1) or "rms" (0) */
    int32_t count;    /* submissions behind the answer */
} sel_dvb_loudness;

/* md5 of the file bytes as 32 lower-case hex characters; 1 on success. */
int sel_dvb_loudness_file_hash(const char* path, char hex[33]);

/* GET /api/loudness/<hash>; fills *out on HIT. *requests (optional) counts transport requests. */
sel_dvb_loudness_status sel_dvb_loudness_get(const char* hash, sel_dvb_loudness* out, unsigned* requests);

/* PUT /api/loudness/<hash> with the bearer token; returns the HTTP status (200 ok), 0 on transport failure. */
int sel_dvb_loudness_put(const char* hash, const char* token, const sel_dvb_loudness* in, const char* analyser);

/* Gain in centi-dB for a measured integrated loudness: target - lufs, limited to +-24 dB (the analyser's rule). */
int32_t sel_dvb_loudness_gain_cdb(int32_t lufs_cdb);

#endif
