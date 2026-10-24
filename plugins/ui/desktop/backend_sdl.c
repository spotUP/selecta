/* The `sdl` backend of the desktop UI (DUI-P2-7); see backend_sdl.h. */
#include "backend_sdl.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

struct desk_sdl {
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
    int32_t w, h;
    int32_t scale; /* the scale the canvas was last sized for */
    int headless;
    uint64_t present_hash;
    uint32_t upload_pixels;
    uint32_t presents;
};

static int make_texture(desk_sdl* b, int32_t w, int32_t h) {
    SDL_Texture* t = SDL_CreateTexture(b->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (t == NULL) {
        fprintf(stderr, "selecta: SDL texture %dx%d failed: %s\n", (int)w, (int)h, SDL_GetError());
        return 0;
    }
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    if (b->texture != NULL) {
        SDL_DestroyTexture(b->texture);
    }
    b->texture = t;
    b->w = w;
    b->h = h;
    SDL_SetRenderLogicalPresentation(b->renderer, w, h, SDL_LOGICAL_PRESENTATION_STRETCH);
    return 1;
}

desk_sdl* desk_sdl_open(int32_t w, int32_t h) {
    desk_sdl* b;
    const char* drv;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "selecta: SDL video init failed: %s\n", SDL_GetError());
        return NULL;
    }
    b = (desk_sdl*)SDL_calloc(1, sizeof *b);
    if (b == NULL) {
        return NULL;
    }
    b->scale = 1;
    drv = SDL_GetCurrentVideoDriver();
    b->headless = drv != NULL && (strcmp(drv, "offscreen") == 0 || strcmp(drv, "dummy") == 0);
    b->window = SDL_CreateWindow("Selecta", w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (b->window != NULL) { /* the offscreen and dummy drivers have no GL: render in software */
        b->renderer = SDL_CreateRenderer(b->window, b->headless ? "software" : NULL);
    }
    if (b->renderer == NULL || !make_texture(b, w, h)) {
        if (b->renderer == NULL) {
            fprintf(stderr, "selecta: SDL %s failed: %s\n", b->window == NULL ? "window" : "renderer", SDL_GetError());
        }
        desk_sdl_close(b);
        return NULL;
    }
    return b;
}

void desk_sdl_close(desk_sdl* b) {
    if (b == NULL) {
        return;
    }
    if (b->texture != NULL) {
        SDL_DestroyTexture(b->texture);
    }
    if (b->renderer != NULL) {
        SDL_DestroyRenderer(b->renderer);
    }
    if (b->window != NULL) {
        SDL_DestroyWindow(b->window);
    }
    SDL_free(b);
    SDL_Quit();
}

int desk_sdl_headless(const desk_sdl* b) {
    return b->headless;
}

int32_t desk_sdl_scale(const desk_sdl* b) {
    float density = SDL_GetWindowPixelDensity(b->window);
    int32_t scale = density > 1.0f ? (int32_t)density + (density > (float)(int32_t)density ? 1 : 0) : 1;
    return scale > 3 ? 3 : scale; /* a fractional density rounds the layout scale up and SDL scales the texture down */
}

int desk_sdl_resize(desk_sdl* b, int32_t w, int32_t h) {
    int pw = 0, ph = 0;
    int32_t scale = desk_sdl_scale(b);
    SDL_GetWindowSizeInPixels(b->window, &pw, &ph);
    if (pw != w || ph != h) { /* a user resize already sized the window; only a scripted one changes it */
        SDL_SetWindowSize(b->window, w / scale, h / scale);
        SDL_SyncWindow(b->window);
    }
    b->scale = scale;
    return make_texture(b, w, h);
}

/* Hashes a rendered frame with the one canvas hash, by putting it in a canvas value. */
static uint64_t hash_surface(const SDL_Surface* s, int32_t w, int32_t h) {
    sel_gui_canvas* tmp = (sel_gui_canvas*)SDL_malloc(sizeof *tmp);
    uint32_t* copy = (uint32_t*)SDL_malloc(sel_gui_pixel_bytes(SEL_GUI_RGBA32, w, h));
    uint64_t hash = 0u;
    if (tmp != NULL && copy != NULL) {
        int32_t y;
        sel_gui_canvas_init(tmp, SEL_GUI_RGBA32, w, h, copy);
        for (y = 0; y < h; y++) {
            memcpy(copy + (size_t)y * (size_t)w, (const uint8_t*)s->pixels + (size_t)y * (size_t)s->pitch, (size_t)w * 4u);
        }
        hash = sel_gui_hash(tmp);
    }
    SDL_free(tmp);
    SDL_free(copy);
    return hash;
}

void desk_sdl_present(desk_sdl* b, const sel_gui_canvas* c, int readback) {
    const uint32_t* px = (const uint32_t*)c->pixels;
    uint32_t i;
    for (i = 0; i < c->dirty_n; i++) {
        SDL_Rect r;
        r.x = c->dirty[i].x;
        r.y = c->dirty[i].y;
        r.w = c->dirty[i].w;
        r.h = c->dirty[i].h;
        SDL_UpdateTexture(b->texture, &r, px + (size_t)r.y * (size_t)c->w + (size_t)r.x, c->w * 4);
        b->upload_pixels += (uint32_t)r.w * (uint32_t)r.h;
    }
    SDL_RenderClear(b->renderer);
    SDL_RenderTexture(b->renderer, b->texture, NULL, NULL);
    if (readback) {
        SDL_Surface* s = SDL_RenderReadPixels(b->renderer, NULL);
        SDL_Surface* conv = s != NULL ? SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888) : NULL;
        b->present_hash = conv != NULL && conv->w == c->w && conv->h == c->h ? hash_surface(conv, c->w, c->h) : 0u;
        SDL_DestroySurface(conv);
        SDL_DestroySurface(s);
    }
    SDL_RenderPresent(b->renderer);
    b->presents++;
}

uint64_t desk_sdl_present_hash(const desk_sdl* b) {
    return b->present_hash;
}

uint32_t desk_sdl_upload_pixels(const desk_sdl* b) {
    return b->upload_pixels;
}

uint32_t desk_sdl_presents(const desk_sdl* b) {
    return b->presents;
}

int desk_sdl_poll(desk_sdl* b, uint32_t timeout_ms, int32_t* w, int32_t* h) {
    SDL_Event ev;
    int result = DESK_SDL_NONE;
    int got = timeout_ms > 0u ? SDL_WaitEventTimeout(&ev, (int32_t)timeout_ms) : SDL_PollEvent(&ev);
    while (got) {
        switch (ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            return DESK_SDL_QUIT;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: /* a resize, or a move between screens of another scale */
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED: {
            int pw = 0, ph = 0;
            SDL_GetWindowSizeInPixels(b->window, &pw, &ph);
            if (pw > 0 && ph > 0 && (pw != b->w || ph != b->h || desk_sdl_scale(b) != b->scale)) {
                *w = pw;
                *h = ph;
                result = DESK_SDL_RESIZE;
            } else if (result == DESK_SDL_NONE) {
                result = DESK_SDL_ACTIVITY;
            }
            break;
        }
        default:
            break;
        }
        got = SDL_PollEvent(&ev);
    }
    return result;
}

/* Milliseconds since SDL started: the live UI clock (DUI-P3-5). */
uint32_t desk_sdl_ticks(void) {
    return (uint32_t)SDL_GetTicks();
}
