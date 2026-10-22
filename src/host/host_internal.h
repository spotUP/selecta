#ifndef SELECTA_HOST_INTERNAL_H
#define SELECTA_HOST_INTERNAL_H

#include <selecta/host.h>

void* sel_host_mem_alloc(const sel_host* h, size_t size, uint32_t kind);
void sel_host_mem_free(const sel_host* h, void* p);
void sel_host_log(const sel_host* h, int level, const char* msg);
uint32_t sel_host_clock_ms(const sel_host* h);

#endif
