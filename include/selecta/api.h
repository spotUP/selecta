/* Selecta plugin SDK: shared definitions every header needs.
 * Design doc: thoughts/shared/plans/2026-10-22-architecture-design.md, section 10.
 * C99, compiles as C++, no allocation, no OS headers. */
#ifndef SELECTA_API_H
#define SELECTA_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#define SEL_EXTERN_C_BEGIN extern "C" {
#define SEL_EXTERN_C_END }
#else
#define SEL_EXTERN_C_BEGIN
#define SEL_EXTERN_C_END
#endif

/* Bumped only for an incompatible change to sel_host or the tag ABI.
 * Appending a field to sel_host does not bump it (the size field covers that). */
#define SEL_API_VERSION 1u

#define SEL_VERSION_STRING "0.0.1"

SEL_EXTERN_C_BEGIN

/* Version line shared by every app: "<name> 0.0.1 api 1". */
const char* sel_version_string(void);

SEL_EXTERN_C_END

#endif /* SELECTA_API_H */
