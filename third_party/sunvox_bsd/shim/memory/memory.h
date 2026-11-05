/* Selecta shim for SunDog "memory/memory.h": zeroed blocks with a size header, one heap. */
#ifndef __MEMORY__
#define __MEMORY__
#include "../core/core.h"
enum { HEAP_DYNAMIC = 0, HEAP_STORAGE };
#define MEM_NEW(heap, size) mem_new(heap, size, (UTF8_CHAR*)__FUNCTION__, 0)
void* mem_new(unsigned long heap, unsigned long size, const UTF8_CHAR* name, unsigned long id);
void mem_free(void* ptr);
void mem_set(void* ptr, unsigned long size, unsigned char value);
void* mem_resize(void* ptr, int size);
void mem_copy(void* dest, const void* src, unsigned long size);
int mem_cmp(const char* p1, const char* p2, unsigned long size);
void mem_strcat(char* dest, const char* src);
long mem_strcmp(const char* s1, const char* s2);
int mem_strlen(const char* s);
char* mem_strdup(const char* s1);
long mem_get_size(void* ptr);
void mem_on(void);
void mem_off(void);
#endif
