/* Audio-thread rule (design doc section 3): the thread that calls
 * sel_engine_render once an output marks it is the audio thread. In debug
 * builds render asserts it always runs there and the command side
 * (create, send, pump, set_mix, destroy) asserts it never does, because the
 * audio thread must not block, allocate or take locks. Release builds and
 * the Amiga build carry no checks. */
#ifndef SELECTA_ENGINE_THREAD_H
#define SELECTA_ENGINE_THREAD_H

#if !defined(NDEBUG) && (defined(__APPLE__) || defined(__linux__))
#include <assert.h>
#include <pthread.h>

#define SEL_THREAD_CHECKS 1
#define SEL_THREAD_FIELDS \
    volatile int audio_marked; \
    pthread_t audio_thread;
#define SEL_THREAD_MARK_AUDIO(e) \
    do { \
        (e)->audio_thread = pthread_self(); \
        (e)->audio_marked = 1; \
    } while (0)
/* The output that pulled audio is gone (I219 handover): no thread is the audio thread until one marks again. */
#define SEL_THREAD_UNMARK_AUDIO(e) ((e)->audio_marked = 0)
/* SEL_THREAD_AUDIO: this code runs on the audio thread. */
#define SEL_THREAD_AUDIO(e) assert(!(e)->audio_marked || pthread_equal((e)->audio_thread, pthread_self()))
/* SEL_THREAD_NOT_AUDIO: this code must stay off the audio thread. */
#define SEL_THREAD_NOT_AUDIO(e) \
    assert(!(e)->audio_marked || !pthread_equal((e)->audio_thread, pthread_self()))
#else
#define SEL_THREAD_FIELDS
#define SEL_THREAD_MARK_AUDIO(e) ((void)(e))
#define SEL_THREAD_UNMARK_AUDIO(e) ((void)(e))
#define SEL_THREAD_AUDIO(e) ((void)(e))
#define SEL_THREAD_NOT_AUDIO(e) ((void)(e))
#endif

#endif
