/* Krakatoa Smoke on the SDL3 GPU API. See krk_smoke_gpu.h. Shaders: shaders/krk_smoke.comp, compiled per pass to MSL at
 * build time (cmake/krk_embed.cmake) into krk_shaders_msl.h. Metal buffer order is SDL_GPU's: [[buffer(0)]] the
 * uniform block, then the read-only storage buffers, then the read-write ones; a read-write set is fixed per compute
 * pass, so every kernel gets its own pass. */
#include "krk_smoke_gpu.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "krk_shaders_msl.h"

enum { P_ADVECT, P_CURL, P_FORCE, P_DIV, P_JACOBI, P_GRAD, P_MARCH, P_COUNT };

static const struct {
    const unsigned char* code;
    size_t len;
    uint32_t ro, rw, tx, ty, tz;
} k_pass[P_COUNT] = {
    {krk_msl_advect, sizeof krk_msl_advect, 2, 2, 4, 4, 4}, {krk_msl_curl, sizeof krk_msl_curl, 1, 1, 4, 4, 4},
    {krk_msl_force, sizeof krk_msl_force, 2, 1, 4, 4, 4},   {krk_msl_div, sizeof krk_msl_div, 1, 1, 4, 4, 4},
    {krk_msl_jacobi, sizeof krk_msl_jacobi, 2, 1, 4, 4, 4}, {krk_msl_grad, sizeof krk_msl_grad, 1, 1, 4, 4, 4},
    {krk_msl_march, sizeof krk_msl_march, 1, 1, 8, 8, 1},
};

struct krk_gpu {
    SDL_GPUDevice* dev;
    SDL_GPUComputePipeline* pipe[P_COUNT];
    SDL_GPUBuffer *vel[2], *sca[2], *curl, *div, *p[2], *out;
    SDL_GPUTransferBuffer* down;
    krk_grid g;
    uint32_t ow, oh;
    int cur, pcur;
};

static SDL_GPUBuffer* mk_buf(SDL_GPUDevice* d, uint32_t size) {
    SDL_GPUBufferCreateInfo bi;
    memset(&bi, 0, sizeof bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ | SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
    bi.size = size;
    return SDL_CreateGPUBuffer(d, &bi);
}

/* GPU buffers start undefined: upload zeros once so the first step starts from still, empty air. */
static int zero_all(krk_gpu* k, SDL_GPUBuffer** bufs, const uint32_t* sizes, uint32_t n, uint32_t maxsize) {
    SDL_GPUTransferBufferCreateInfo ti;
    SDL_GPUTransferBuffer* up;
    SDL_GPUCommandBuffer* cmd;
    SDL_GPUCopyPass* cp;
    void* map;
    uint32_t i;
    memset(&ti, 0, sizeof ti);
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    ti.size = maxsize;
    up = SDL_CreateGPUTransferBuffer(k->dev, &ti);
    if (up == NULL) {
        return 0;
    }
    map = SDL_MapGPUTransferBuffer(k->dev, up, false);
    if (map == NULL) {
        SDL_ReleaseGPUTransferBuffer(k->dev, up);
        return 0;
    }
    memset(map, 0, maxsize);
    SDL_UnmapGPUTransferBuffer(k->dev, up);
    cmd = SDL_AcquireGPUCommandBuffer(k->dev);
    if (cmd == NULL) {
        SDL_ReleaseGPUTransferBuffer(k->dev, up);
        return 0;
    }
    cp = SDL_BeginGPUCopyPass(cmd);
    for (i = 0; i < n; i++) {
        SDL_GPUTransferBufferLocation src = {up, 0};
        SDL_GPUBufferRegion dst = {bufs[i], 0, sizes[i]};
        SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    }
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_WaitForGPUIdle(k->dev);
    SDL_ReleaseGPUTransferBuffer(k->dev, up);
    return 1;
}

krk_gpu* krk_gpu_create(const krk_grid* g, uint32_t out_w, uint32_t out_h) {
    krk_gpu* k;
    uint32_t n = g->x * g->y * g->z, i, outsz = out_w * out_h * 4u;
    SDL_GPUTransferBufferCreateInfo ti;
    SDL_GPUBuffer* all[10];
    uint32_t sizes[10];
    if (n == 0u || out_w == 0u || out_h == 0u) {
        return NULL;
    }
    k = (krk_gpu*)calloc(1, sizeof *k);
    if (k == NULL) {
        return NULL;
    }
    k->g = *g;
    k->ow = out_w;
    k->oh = out_h;
    /* SDL_CreateGPUDevice needs the video subsystem. The SDL window backend has it up already, but the mem backend
       (tests, headless runs) never starts it, so the device failed there. Take a counted reference here and drop it
       in krk_gpu_destroy, so the plugin works under any host backend. */
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        SDL_Log("krakatoa: video subsystem: %s", SDL_GetError());
        free(k);
        return NULL;
    }
    k->dev = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, NULL);
    if (k->dev == NULL) {
        SDL_Log("krakatoa: GPU device: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        free(k);
        return NULL;
    }
    for (i = 0; i < P_COUNT; i++) {
        SDL_GPUComputePipelineCreateInfo ci;
        memset(&ci, 0, sizeof ci);
        ci.code = k_pass[i].code;
        ci.code_size = k_pass[i].len - 1u; /* the generated array carries a trailing NUL */
        ci.entrypoint = "main0";
        ci.format = SDL_GPU_SHADERFORMAT_MSL;
        ci.num_readonly_storage_buffers = k_pass[i].ro;
        ci.num_readwrite_storage_buffers = k_pass[i].rw;
        ci.num_uniform_buffers = 1;
        ci.threadcount_x = k_pass[i].tx;
        ci.threadcount_y = k_pass[i].ty;
        ci.threadcount_z = k_pass[i].tz;
        k->pipe[i] = SDL_CreateGPUComputePipeline(k->dev, &ci);
        if (k->pipe[i] == NULL) {
            SDL_Log("krakatoa: pipeline %u: %s", (unsigned)i, SDL_GetError());
            krk_gpu_destroy(k);
            return NULL;
        }
    }
    all[0] = k->vel[0] = mk_buf(k->dev, n * 16u);
    all[1] = k->vel[1] = mk_buf(k->dev, n * 16u);
    all[2] = k->sca[0] = mk_buf(k->dev, n * 16u);
    all[3] = k->sca[1] = mk_buf(k->dev, n * 16u);
    all[4] = k->curl = mk_buf(k->dev, n * 16u);
    all[5] = k->div = mk_buf(k->dev, n * 4u);
    all[6] = k->p[0] = mk_buf(k->dev, n * 4u);
    all[7] = k->p[1] = mk_buf(k->dev, n * 4u);
    all[8] = k->out = mk_buf(k->dev, outsz);
    for (i = 0; i < 9u; i++) {
        sizes[i] = i < 5u ? n * 16u : i < 8u ? n * 4u : outsz;
        if (all[i] == NULL) {
            krk_gpu_destroy(k);
            return NULL;
        }
    }
    memset(&ti, 0, sizeof ti);
    ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    ti.size = outsz;
    k->down = SDL_CreateGPUTransferBuffer(k->dev, &ti);
    if (k->down == NULL || !zero_all(k, all, sizes, 8u, n * 16u)) {
        krk_gpu_destroy(k);
        return NULL;
    }
    return k;
}

static void run(SDL_GPUCommandBuffer* cmd, krk_gpu* k, int pass, SDL_GPUBuffer* const* ro, SDL_GPUBuffer* const* rw,
                const krk_uniforms* u) {
    SDL_GPUStorageBufferReadWriteBinding rwb[2];
    SDL_GPUComputePass* cp;
    uint32_t i, gx, gy, gz;
    memset(rwb, 0, sizeof rwb);
    for (i = 0; i < k_pass[pass].rw; i++) {
        rwb[i].buffer = rw[i];
    }
    cp = SDL_BeginGPUComputePass(cmd, NULL, 0, rwb, k_pass[pass].rw);
    SDL_BindGPUComputePipeline(cp, k->pipe[pass]);
    SDL_BindGPUComputeStorageBuffers(cp, 0, ro, k_pass[pass].ro);
    SDL_PushGPUComputeUniformData(cmd, 0, u, (Uint32)sizeof *u);
    if (pass == P_MARCH) {
        gx = (k->ow + 7u) / 8u;
        gy = (k->oh + 7u) / 8u;
        gz = 1u;
    } else {
        gx = k->g.x / 4u;
        gy = k->g.y / 4u;
        gz = (k->g.z + 3u) / 4u;
    }
    SDL_DispatchGPUCompute(cp, gx, gy, gz);
    SDL_EndGPUComputePass(cp);
}

uint32_t krk_gpu_step(krk_gpu* k, const krk_uniforms* uin, uint32_t jacobi, uint32_t* out) {
    SDL_GPUCommandBuffer* cmd;
    SDL_GPUCopyPass* copy;
    SDL_GPUFence* fence;
    krk_uniforms u = *uin;
    uint32_t it, outsz = k->ow * k->oh * 4u;
    int c = k->cur, pc = k->pcur;
    const void* map;
    cmd = SDL_AcquireGPUCommandBuffer(k->dev);
    if (cmd == NULL) {
        return 0;
    }
    u.dim[3] = 0;
    {
        SDL_GPUBuffer* ro[2] = {k->vel[c], k->sca[c]};
        SDL_GPUBuffer* rw[2] = {k->vel[c ^ 1], k->sca[c ^ 1]};
        run(cmd, k, P_ADVECT, ro, rw, &u);
        c ^= 1;
    }
    run(cmd, k, P_CURL, &k->vel[c], &k->curl, &u);
    {
        SDL_GPUBuffer* ro[2] = {k->curl, k->sca[c]};
        run(cmd, k, P_FORCE, ro, &k->vel[c], &u);
    }
    run(cmd, k, P_DIV, &k->vel[c], &k->div, &u);
    for (it = 0; it < jacobi; it++) {
        SDL_GPUBuffer* ro[2] = {k->div, k->p[pc]};
        u.dim[3] = (int32_t)it;
        run(cmd, k, P_JACOBI, ro, &k->p[pc ^ 1], &u);
        pc ^= 1;
    }
    u.dim[3] = 0;
    run(cmd, k, P_GRAD, &k->p[pc], &k->vel[c], &u);
    run(cmd, k, P_MARCH, &k->sca[c], &k->out, &u);
    copy = SDL_BeginGPUCopyPass(cmd);
    {
        SDL_GPUBufferRegion src = {k->out, 0, outsz};
        SDL_GPUTransferBufferLocation dst = {k->down, 0};
        SDL_DownloadFromGPUBuffer(copy, &src, &dst);
    }
    SDL_EndGPUCopyPass(copy);
    fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence == NULL) {
        return 0;
    }
    SDL_WaitForGPUFences(k->dev, true, &fence, 1);
    SDL_ReleaseGPUFence(k->dev, fence);
    k->cur = c;
    k->pcur = pc;
    map = SDL_MapGPUTransferBuffer(k->dev, k->down, false);
    if (map == NULL) {
        return 0;
    }
    memcpy(out, map, outsz);
    SDL_UnmapGPUTransferBuffer(k->dev, k->down);
    return jacobi;
}

void krk_gpu_destroy(krk_gpu* k) {
    uint32_t i;
    SDL_GPUBuffer* bufs[9];
    if (k == NULL) {
        return;
    }
    if (k->dev != NULL) {
        bufs[0] = k->vel[0];
        bufs[1] = k->vel[1];
        bufs[2] = k->sca[0];
        bufs[3] = k->sca[1];
        bufs[4] = k->curl;
        bufs[5] = k->div;
        bufs[6] = k->p[0];
        bufs[7] = k->p[1];
        bufs[8] = k->out;
        for (i = 0; i < 9u; i++) {
            if (bufs[i] != NULL) {
                SDL_ReleaseGPUBuffer(k->dev, bufs[i]);
            }
        }
        for (i = 0; i < P_COUNT; i++) {
            if (k->pipe[i] != NULL) {
                SDL_ReleaseGPUComputePipeline(k->dev, k->pipe[i]);
            }
        }
        if (k->down != NULL) {
            SDL_ReleaseGPUTransferBuffer(k->dev, k->down);
        }
        SDL_DestroyGPUDevice(k->dev);
        SDL_QuitSubSystem(SDL_INIT_VIDEO); /* the reference krk_gpu_create took */
    }
    free(k);
}
