/* Selecta shim for SunDog "filesystem/v3nus_fs.h": read-only files over one memory buffer set by sunvox_shim_set_song(). */
#ifndef __V3NUS_FS__
#define __V3NUS_FS__
#include "../core/core.h"
typedef uintptr_t V3_FILE_T;
#define V3_FILE V3_FILE_T
V3_FILE v3_open(const UTF8_CHAR* filename, const UTF8_CHAR* filemode);
int v3_close(V3_FILE f);
int v3_seek(V3_FILE f, long offset, long access);
int v3_eof(V3_FILE f);
ulong v3_read(void* ptr, ulong el_size, ulong elements, V3_FILE f);
ulong v3_write(void* ptr, ulong el_size, ulong elements, V3_FILE f);
void v3_putc(int val, V3_FILE f);
void sunvox_shim_set_song(const unsigned char* data, unsigned long size);
#endif
