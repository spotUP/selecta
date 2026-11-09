/* Krakatoa Smoke on the SDL3 GPU API (I1052, design section 2, modern tier). The header stays free of SDL so the
 * plugin sources keep SDK isolation; krk_smoke_gpu.c is built as its own library (plugins/viz/krakatoa/CMakeLists.txt).
 * One step = advect, curl, force, divergence, `jacobi` pressure iterations, gradient subtract, ray march, then a readback
 * of the out_w x out_h image as 0xAARRGGBB. */
#ifndef KRK_SMOKE_GPU_H
#define KRK_SMOKE_GPU_H

#include <stdint.h>

#include "../core/krk_core.h"

typedef struct krk_gpu krk_gpu;

/* NULL when no GPU device or pipeline can be made (the plugin then draws nothing and says gpu 0 in its stats). */
krk_gpu* krk_gpu_create(const krk_grid* g, uint32_t out_w, uint32_t out_h);
/* Runs one step under u (u->dim[3] is set per Jacobi dispatch) and writes out_w * out_h pixels to out. Returns the number
 * of Jacobi dispatches recorded, 0 on failure. */
uint32_t krk_gpu_step(krk_gpu* k, const krk_uniforms* u, uint32_t jacobi, uint32_t* out);
void krk_gpu_destroy(krk_gpu* k);

#endif /* KRK_SMOKE_GPU_H */
