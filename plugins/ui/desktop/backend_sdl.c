/* The `sdl` backend of the desktop UI (DUI-P2-7); see backend_sdl.h. */
#include "backend_sdl.h"

#include <selecta/uimodel/keys.h>

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DESK_SDL_QUEUE 64u

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
    sel_gui_input_map_state map; /* press state of the pointer buttons */
    sel_gui_input queue[DESK_SDL_QUEUE];
    uint32_t q_head, q_count;
    uint32_t drop_n;          /* files of the drop in progress so far (SDL_EVENT_DROP_BEGIN resets it) */
    char drop_hold[SEL_GUI_INPUT_PATH]; /* the newest file of the drop, sent at the next file or at DROP_COMPLETE */
    int drop_held;
    char dlg_path[SEL_GUI_INPUT_PATH]; /* the folder the open dialog answered; read on the main thread in desk_sdl_poll */
    SDL_AtomicInt dlg_ready;
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

/* Test hook: SELECTA_SDL_TEST_EVENTS is a space separated list of events pushed on the SDL queue at open, so a test can
 * drive the real desk_sdl_poll. `k<keycode hex>/<keymod hex>` is a KEY_DOWN, `t<text>` a TEXT_INPUT. */
static void push_test_events(const char* list) {
    char buf[256];
    char* save = NULL;
    char* tok;
    if (list == NULL) {
        return;
    }
    snprintf(buf, sizeof buf, "%s", list);
    for (tok = strtok_r(buf, " ", &save); tok != NULL; tok = strtok_r(NULL, " ", &save)) {
        SDL_Event ev;
        memset(&ev, 0, sizeof ev);
        if (tok[0] == 'k') {
            unsigned long key = 0ul, mod = 0ul;
            if (sscanf(tok + 1, "%lx/%lx", &key, &mod) < 1) {
                continue;
            }
            ev.type = SDL_EVENT_KEY_DOWN;
            ev.key.down = true;
            ev.key.key = (SDL_Keycode)key;
            ev.key.mod = (SDL_Keymod)mod;
        } else if (tok[0] == 't') {
            ev.type = SDL_EVENT_TEXT_INPUT;
            ev.text.text = SDL_strdup(tok + 1); /* the test hook never frees: it runs once per process */
        } else {
            continue;
        }
        SDL_PushEvent(&ev);
    }
}

/* Test hook: SELECTA_SDL_TEST_TIMED is a space separated list of events pushed from an SDL timer thread while the live
 * loop waits in poll: `m<x>,<y>@<ms>` a mouse motion, `d<btn>,<x>,<y>@<ms>` a mouse button down, `u<btn>,<x>,<y>@<ms>` a button up (1 left, 3 right),
 * `q@<ms>` a quit, <ms> after open. SELECTA_SDL_TEST_LIVE=1 makes the
 * dummy driver count as a live display, so a test runs the real live loop (I341). */
static Uint32 SDLCALL timed_event(void* param, SDL_TimerID id, Uint32 interval) {
    SDL_Event* ev = (SDL_Event*)param;
    (void)id;
    (void)interval;
    SDL_PushEvent(ev);
    return 0u;
}

static void push_timed_events(const char* list) {
    char buf[256];
    char* save = NULL;
    char* tok;
    if (list == NULL) {
        return;
    }
    snprintf(buf, sizeof buf, "%s", list);
    for (tok = strtok_r(buf, " ", &save); tok != NULL; tok = strtok_r(NULL, " ", &save)) {
        SDL_Event* ev = (SDL_Event*)SDL_calloc(1, sizeof *ev); /* the test hook never frees: it runs once per process */
        float x = 0.0f, y = 0.0f;
        unsigned ms = 0u, btn = 0u;
        if (ev == NULL) {
            return;
        }
        if (tok[0] == 'm' && sscanf(tok + 1, "%f,%f@%u", &x, &y, &ms) == 3) {
            ev->type = SDL_EVENT_MOUSE_MOTION;
            ev->motion.x = x;
            ev->motion.y = y;
        } else if ((tok[0] == 'd' || tok[0] == 'u') && sscanf(tok + 1, "%u,%f,%f@%u", &btn, &x, &y, &ms) == 4) {
            ev->type = tok[0] == 'd' ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            ev->button.button = (Uint8)btn;
            ev->button.down = tok[0] == 'd';
            ev->button.clicks = 1;
            ev->button.x = x;
            ev->button.y = y;
        } else if (tok[0] == 'q' && sscanf(tok + 1, "@%u", &ms) == 1) {
            ev->type = SDL_EVENT_QUIT;
        } else {
            continue;
        }
        (void)SDL_AddTimer(ms, timed_event, ev);
    }
}

void desk_sdl_default_size(int32_t* w, int32_t* h) {
    const char* env = getenv("SELECTA_SDL_SCREEN");
    int ew, eh;
    *w = 1280;
    *h = 800;
    if (env != NULL && sscanf(env, "%dx%d", &ew, &eh) == 2 && ew >= 64 && eh >= 64) {
        *w = ew;
        *h = eh;
    }
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
    SDL_StartTextInput(b->window); /* typed characters arrive as SDL_EVENT_TEXT_INPUT, layout and shift already applied */
    push_test_events(getenv("SELECTA_SDL_TEST_EVENTS"));
    push_timed_events(getenv("SELECTA_SDL_TEST_TIMED"));
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
    const char* live = getenv("SELECTA_SDL_TEST_LIVE");
    return b->headless && !(live != NULL && live[0] == '1');
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

static int32_t q16_of(float v) {
    return (int32_t)SDL_lroundf(v * 65536.0f);
}

/* One raw pointer event through the shared mapping into the queue; returns 1 when an event was queued. */
static int queue_raw(desk_sdl* b, const sel_gui_raw* raw) {
    sel_gui_input in;
    if (!sel_gui_input_map(&b->map, raw, &in) || b->q_count >= DESK_SDL_QUEUE) {
        return 0;
    }
    b->queue[(b->q_head + b->q_count) % DESK_SDL_QUEUE] = in;
    b->q_count++;
    return 1;
}

/* A dropped file or folder as a SEL_GUI_IN_DROP: x is 0 for a lone file (the UI plays it) and 1 for a file of a drop of
 * several (the UI queues each). */
static int queue_drop(desk_sdl* b, const char* path, int32_t more) {
    sel_gui_input in;
    if (path == NULL || b->q_count >= DESK_SDL_QUEUE) {
        return 0;
    }
    memset(&in, 0, sizeof in);
    in.kind = SEL_GUI_IN_DROP;
    in.x = more;
    SDL_strlcpy(in.path, path, sizeof in.path);
    b->queue[(b->q_head + b->q_count) % DESK_SDL_QUEUE] = in;
    b->q_count++;
    return 1;
}

static void SDLCALL folder_chosen(void* userdata, const char* const* list, int filter) {
    desk_sdl* b = (desk_sdl*)userdata;
    (void)filter;
    if (list != NULL && list[0] != NULL) {
        SDL_strlcpy(b->dlg_path, list[0], sizeof b->dlg_path);
        SDL_SetAtomicInt(&b->dlg_ready, 1);
    }
}

int desk_sdl_open_folder_dialog(desk_sdl* b) {
    if (b == NULL || b->window == NULL || b->headless) {
        return 0;
    }
    SDL_ShowOpenFolderDialog(folder_chosen, b, b->window, NULL, 0);
    return 1;
}

int desk_sdl_next_input(desk_sdl* b, sel_gui_input* ev) {
    if (b->q_count == 0u) {
        return 0;
    }
    *ev = b->queue[b->q_head];
    b->q_head = (b->q_head + 1u) % DESK_SDL_QUEUE;
    b->q_count--;
    return 1;
}

/* TXT_MOD_* of an SDL modifier state. Command counts as Control on macOS (the platform's shortcut key); the key grammar
 * has no meta modifier. */
static uint32_t txt_mods_of(SDL_Keymod m) {
    uint32_t mods = 0u;
    if ((m & SDL_KMOD_SHIFT) != 0) {
        mods |= TXT_MOD_SHIFT;
    }
    if ((m & SDL_KMOD_ALT) != 0) {
        mods |= TXT_MOD_ALT;
    }
    if ((m & SDL_KMOD_CTRL) != 0) {
        mods |= TXT_MOD_CTRL;
    }
#ifdef __APPLE__
    if ((m & SDL_KMOD_GUI) != 0) {
        mods |= TXT_MOD_CTRL;
    }
#endif
    return mods;
}

/* The key code (TXT_KEY_* or a code point) a KEY_DOWN delivers, or 0 when the key is a modifier, unknown, or a
 * printable one that arrives as text instead. Printable keys are delivered here only under Control or Alt. */
static uint32_t txt_code_of(SDL_Keycode k, uint32_t mods) {
    switch (k) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return 13u;
    case SDLK_ESCAPE: return 27u;
    case SDLK_TAB: return 9u;
    case SDLK_BACKSPACE: return 127u;
    case SDLK_UP: return TXT_KEY_UP;
    case SDLK_DOWN: return TXT_KEY_DOWN;
    case SDLK_LEFT: return TXT_KEY_LEFT;
    case SDLK_RIGHT: return TXT_KEY_RIGHT;
    case SDLK_HOME: return TXT_KEY_HOME;
    case SDLK_END: return TXT_KEY_END;
    case SDLK_PAGEUP: return TXT_KEY_PAGE_UP;
    case SDLK_PAGEDOWN: return TXT_KEY_PAGE_DOWN;
    case SDLK_INSERT: return TXT_KEY_INSERT;
    case SDLK_DELETE: return TXT_KEY_DELETE;
    default: break;
    }
    if (k >= SDLK_F1 && k <= SDLK_F12) {
        return TXT_KEY_F1 + (uint32_t)(k - SDLK_F1);
    }
    if (k >= 0x20 && k <= 0x7e && (mods & (TXT_MOD_CTRL | TXT_MOD_ALT)) != 0u) {
        return (uint32_t)k;
    }
    return 0u;
}

/* One KEY_DOWN or TEXT_INPUT event through the shared mapping into the queue. */
static int queue_key(desk_sdl* b, const SDL_Event* ev) {
    sel_gui_raw raw;
    memset(&raw, 0, sizeof raw);
    if (ev->type == SDL_EVENT_KEY_DOWN) {
        raw.kind = SEL_GUI_RAW_KEY;
        raw.mods = txt_mods_of(ev->key.mod);
        raw.code = txt_code_of(ev->key.key, raw.mods);
        return queue_raw(b, &raw);
    }
    /* Text typed while Control, Command or Alt is held is the key chord, which KEY_DOWN already delivered. */
    if ((SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0 || ev->text.text == NULL) {
        return 0;
    }
    {
        const char* t = ev->text.text;
        int queued = 0;
        while (*t != '\0') {
            size_t len = SDL_strlen(t);
            Uint32 cp = SDL_StepUTF8(&t, &len);
            if (cp == 0u) {
                break;
            }
            raw.kind = SEL_GUI_RAW_TEXT;
            raw.code = cp;
            queued |= queue_raw(b, &raw);
        }
        return queued;
    }
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
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_WHEEL:
        case SDL_EVENT_PINCH_UPDATE: {
            sel_gui_raw raw;
            memset(&raw, 0, sizeof raw);
            SDL_ConvertEventToRenderCoordinates(b->renderer, &ev);
            if (ev.type == SDL_EVENT_MOUSE_MOTION) {
                raw.kind = SEL_GUI_RAW_MOTION;
                raw.x = (int32_t)ev.motion.x;
                raw.y = (int32_t)ev.motion.y;
            } else if (ev.type == SDL_EVENT_MOUSE_WHEEL) {
                float dir = ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
                raw.kind = SEL_GUI_RAW_WHEEL;
                raw.dx = q16_of(ev.wheel.x * dir);
                raw.dy = q16_of(ev.wheel.y * dir);
            } else if (ev.type == SDL_EVENT_PINCH_UPDATE) {
                raw.kind = SEL_GUI_RAW_PINCH;
                raw.scale = q16_of(ev.pinch.scale);
            } else {
                raw.kind = ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? SEL_GUI_RAW_BUTTON_DOWN : SEL_GUI_RAW_BUTTON_UP;
                raw.x = (int32_t)ev.button.x;
                raw.y = (int32_t)ev.button.y;
                raw.button = ev.button.button;
                raw.clicks = ev.button.clicks;
            }
            if (queue_raw(b, &raw) && result == DESK_SDL_NONE) {
                result = DESK_SDL_ACTIVITY;
            }
            break;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_TEXT_INPUT:
            if (queue_key(b, &ev) && result == DESK_SDL_NONE) {
                result = DESK_SDL_ACTIVITY;
            }
            break;
        case SDL_EVENT_DROP_BEGIN:
            b->drop_n = 0u;
            b->drop_held = 0;
            break;
        case SDL_EVENT_DROP_FILE: /* hold one path back: a lone file plays, so it is sent only once DROP_COMPLETE says it is alone */
            if (ev.drop.data != NULL) {
                if (b->drop_held) {
                    (void)queue_drop(b, b->drop_hold, 1);
                }
                SDL_strlcpy(b->drop_hold, ev.drop.data, sizeof b->drop_hold);
                b->drop_held = 1;
                b->drop_n++;
                if (result == DESK_SDL_NONE) {
                    result = DESK_SDL_ACTIVITY;
                }
            }
            break;
        case SDL_EVENT_DROP_COMPLETE:
            if (b->drop_held) {
                (void)queue_drop(b, b->drop_hold, b->drop_n > 1u ? 1 : 0);
                b->drop_held = 0;
            }
            if (result == DESK_SDL_NONE) {
                result = DESK_SDL_ACTIVITY;
            }
            break;
        default:
            break;
        }
        got = SDL_PollEvent(&ev);
    }
    if (SDL_GetAtomicInt(&b->dlg_ready) != 0) { /* the folder dialog answered: the same event a dropped folder gives */
        SDL_SetAtomicInt(&b->dlg_ready, 0);
        if (queue_drop(b, b->dlg_path, 0) && result == DESK_SDL_NONE) {
            result = DESK_SDL_ACTIVITY;
        }
    }
    return result;
}

/* Milliseconds since SDL started: the live UI clock (DUI-P3-5). */
uint32_t desk_sdl_ticks(void) {
    return (uint32_t)SDL_GetTicks();
}
