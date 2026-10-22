/* Debug audio-thread assertion (design doc section 13, verification). Marks
 * the calling thread as the audio thread, then breaks the rule on purpose:
 *   render-off-thread  marks thread A, renders from thread B  (must abort)
 *   send-on-audio      marks this thread, then sends a command (must abort)
 *   render-on-audio    marks this thread, renders there        (control, exit 0)
 * The ctest wrapper tests/thread/thread_assert.cmake expects the abort and the
 * assertion text for the first two and a clean exit for the control. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <selecta/engine.h>
#include <selecta/host.h>

static sel_engine* engine;

static void* render_thread(void* arg) {
    int16_t buf[64 * 2];
    (void)arg;
    sel_engine_render(engine, buf, 64);
    return NULL;
}

int main(int argc, char** argv) {
    const char* mode = argc > 1 ? argv[1] : "";
    int16_t buf[64 * 2];
    sel_message msg;

    engine = sel_engine_create(sel_host_default(), NULL);
    if (!engine) {
        fputs("engine create failed\n", stderr);
        return 2;
    }
    memset(&msg, 0, sizeof msg);
    if (strcmp(mode, "render-off-thread") == 0) {
        pthread_t t;
        sel_engine_mark_audio_thread(engine);
        if (pthread_create(&t, NULL, render_thread, NULL) != 0) {
            return 2;
        }
        pthread_join(t, NULL);
        fputs("assertion did not fire\n", stderr);
        return 1;
    }
    if (strcmp(mode, "send-on-audio") == 0) {
        sel_engine_mark_audio_thread(engine);
        sel_engine_send(engine, &msg);
        fputs("assertion did not fire\n", stderr);
        return 1;
    }
    if (strcmp(mode, "render-on-audio") == 0) {
        sel_engine_mark_audio_thread(engine);
        sel_engine_render(engine, buf, 64);
        puts("render-on-audio ok");
        return 0;
    }
    fputs("usage: thread_assert_test render-off-thread|send-on-audio|render-on-audio\n", stderr);
    return 2;
}
