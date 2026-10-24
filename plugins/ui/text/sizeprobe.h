/* Window size probe of a console (row I218f). The AmigaOS console.device reads "CSI n t" as Set Page
 * Length (n lines), so the xterm size request CSI 18 t must never reach it. The probe asks the native
 * Window Status Request first and sends CSI 18 t only when no native answer came. Pure C99: the
 * terminal I/O is passed in, so a host test drives it with a fake console. */
#ifndef SELECTA_TEXT_SIZEPROBE_H
#define SELECTA_TEXT_SIZEPROBE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct txt_probe_io {
    void* ctx;
    void (*write)(void* ctx, const char* s, size_t n);
    /* Reads the answer bytes into buf, waiting a short time for them; returns the byte count, 0 when
     * nothing came. */
    long (*read)(void* ctx, uint8_t* buf, size_t cap);
} txt_probe_io;

typedef struct txt_probe_result {
    uint32_t rows, cols; /* 0 when no answer */
    int native;          /* the console.device answered (CSI 1;1;rows;cols SPACE r) */
} txt_probe_result;

/* Window Status Request, then (no native answer) xterm CSI 18 t. After a native answer the page
 * length is handed back to the console (parameterless CSI t, console.device "Give Back What You
 * Take") and the size is asked once more. */
txt_probe_result txt_size_probe(const txt_probe_io* io);

/* Parses the bytes of one answer: native CSI 1;1;rows;cols SPACE r or xterm CSI 8;rows;cols t. */
int txt_size_parse(const uint8_t* b, long n, txt_probe_result* out);

#ifdef __cplusplus
}
#endif
#endif
