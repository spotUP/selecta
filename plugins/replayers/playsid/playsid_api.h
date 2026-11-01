/* playsid.library access layer (I434b). The adapter never calls the library directly: it goes through this table so
 * the host test can substitute a stub (the library exists only on the Amiga, as LIBS:playsid.library, and is not
 * bundled: its licence is unresolved, see thoughts/shared/research/2026-11-01-playsid-inventory.md section 4).
 * Register contracts are those of HippoPlayer-68k puu016.s and playsid.library/playsid.asm. */
#ifndef SELECTA_PLAYSID_API_H
#define SELECTA_PLAYSID_API_H

#include <stdint.h>
#include <stddef.h>

/* Operating modes (playsid.library/playsidbase.i OM_*). */
enum {
    PS_OM_NORMAL = 0,
    PS_OM_RESID_6581 = 1,
    PS_OM_RESID_8580 = 2,
    PS_OM_RESID_AUTO = 3,
    PS_OM_SIDBLASTER_USB = 4,
    PS_OM_ZORROSID = 5,
    PS_OM_USBSID_PICO = 6,
    PS_OM_TRINITY = 7,
    PS_OM_COUNT = 8
};

typedef struct ps_ops {
    int (*open)(void);   /* OpenLibrary("playsid.library"); 1 = open (reference counted), 0 = not installed */
    void (*close)(void); /* drops one reference */
    int32_t (*alloc_emul)(void);   /* AllocEmulResource: 0 = ok */
    void (*free_emul)(void);
    void (*set_module)(const void* header, const void* image, uint32_t length); /* a0, a1, d0 */
    int32_t (*start_song)(uint32_t song1);      /* d0 = 1-based subsong; returns error code */
    void (*stop_song)(void);
    void (*pause_song)(void);
    void (*continue_song)(void);
    void (*forward_song)(void);
    void (*set_vert_freq)(uint32_t hz);          /* 50 PAL, 60 NTSC */
    void (*set_operating_mode)(uint32_t om);     /* PS_OM_* */
    void (*set_ahi_mode)(uint32_t ahi_id);       /* 0 = Paula 14-bit */
    uint32_t (*get_operating_mode)(void);
    void (*set_resid_filter)(uint32_t internal_on, uint32_t external_on); /* d0, d1 (I434c) */
    void (*set_resid_boost)(uint32_t boost);     /* d0: 1 = off (0 and 1 are both no-ops), 2-4 = 2x-4x */
    void (*set_resid_mode)(uint32_t oversample); /* d0: 0 = normal, 1-3 = 2x-4x */
} ps_ops;

/* The Amiga build returns the LVO table (playsid_amiga.c); the host test defines its own stub. */
const ps_ops* ps_ops_get(void);

/* Pref value of "playsid.mode" (normal, 6581, 8580, auto, sidblaster, zorrosid, usbsid, trinity; anything else is
 * normal) to the SetOperatingMode argument. */
uint32_t ps_mode_from_pref(const char* value);

/* Pref values of "playsid.filter" (internal, internal+external, off; anything else is internal), "playsid.boost" (off,
 * 2x, 3x, 4x; else off) and "playsid.oversample" (normal, 2x, 3x, 4x; else normal) to the library arguments. The
 * boost result is already the SetRESIDBoost argument (pref index + 1). */
void ps_filter_from_pref(const char* value, uint32_t* internal_on, uint32_t* external_on);
uint32_t ps_boost_from_pref(const char* value);
uint32_t ps_oversample_from_pref(const char* value);

/* True for the modes the reSID engine renders (ForwardSong is skipped there, as in HippoPlayer). */
int ps_mode_is_resid(uint32_t om);

/* Maps the system output ("output.id": ahi, paula, paula14, "" = default) onto the library's SetAHIMode argument:
 * AHI output gives the AHI mode id parsed from `ahi_pref` ("playsid.ahi_mode", hex, optional 0x), every other output
 * gives 0 (Paula 14-bit). One function so the per-format output pref (I888a) extends it in one place. */
uint32_t ps_ahi_mode_for_output(const char* output_id, const char* ahi_pref);

/* SID chips a PSID/RSID header asks for: 1 (mono, v1/v2 or no extra address), 2 (v3+, SID2 address set), 3 (v4+, SID2
 * and SID3 addresses set). `size` is the bytes available at `hdr`; a short or non-SID header counts as 1. */
uint32_t ps_sid_count(const uint8_t* hdr, size_t size);

#endif
