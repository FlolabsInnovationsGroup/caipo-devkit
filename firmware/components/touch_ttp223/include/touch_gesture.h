#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pure gesture classification logic for the TTP223 driver.
 *
 * No ESP-IDF dependencies: time is passed in by the caller, so this file
 * compiles and runs on the host (see firmware/test/host/test_touch_gesture.c).
 * The driver feeds it debounced samples; debouncing itself is the small
 * touch_debounce_* helper below, also pure.
 */

typedef enum {
    TOUCH_GESTURE_PRESS,
    TOUCH_GESTURE_RELEASE,
    TOUCH_GESTURE_TAP,
    TOUCH_GESTURE_DOUBLE_TAP,
    TOUCH_GESTURE_LONG_PRESS,
} touch_gesture_event_t;

typedef struct {
    uint32_t longpress_ms;        /* held this long -> LONG_PRESS (fires while held) */
    uint32_t doubletap_window_ms; /* second press within this window after release -> DOUBLE_TAP */
} touch_gesture_cfg_t;

typedef struct {
    touch_gesture_cfg_t cfg;
    int state;             /* internal */
    uint32_t t_press_ms;   /* internal */
    uint32_t t_release_ms; /* internal */
} touch_gesture_sm_t;

/* A single feed can emit at most this many events (e.g. TAP of the previous
 * gesture + PRESS of the next one in the same sample). */
#define TOUCH_GESTURE_MAX_EVENTS 2

void touch_gesture_init(touch_gesture_sm_t *sm, const touch_gesture_cfg_t *cfg);

/* Feed one debounced sample with its timestamp. Writes 0..TOUCH_GESTURE_MAX_EVENTS
 * events to out[] and returns the count. Call at every poll tick, including
 * when the state did not change — timeouts (TAP, LONG_PRESS) are detected here. */
int touch_gesture_feed(touch_gesture_sm_t *sm, bool pressed, uint32_t now_ms,
                       touch_gesture_event_t out[TOUCH_GESTURE_MAX_EVENTS]);

/* Debouncer: the stable value flips only after `required_samples` consecutive
 * raw samples of the new value. */
typedef struct {
    bool stable;
    bool candidate;
    int count;
    int required;
} touch_debounce_t;

void touch_debounce_init(touch_debounce_t *db, int required_samples, bool initial);
bool touch_debounce_feed(touch_debounce_t *db, bool raw);

#ifdef __cplusplus
}
#endif
