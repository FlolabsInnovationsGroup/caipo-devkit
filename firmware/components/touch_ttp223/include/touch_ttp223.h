#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * TTP223 capacitive touch driver.
 *
 * Polls the sensor's GPIO output (active high), debounces it, and classifies
 * gestures. Events are delivered to a single registered callback from the
 * driver's own task context (never from ISR), so the callback may log, drive
 * the LED, etc. — but should not block for long.
 *
 * Timing (poll interval, debounce, long-press threshold, double-tap window,
 * startup blanking) is configured via menuconfig: Component config →
 * "Caipo touch sensor (TTP223)".
 */

typedef enum {
    TOUCH_EVENT_PRESS,      /* finger down (debounced edge) */
    TOUCH_EVENT_RELEASE,    /* finger up (debounced edge) */
    TOUCH_EVENT_TAP,        /* short press; fires after the double-tap window expires */
    TOUCH_EVENT_DOUBLE_TAP, /* second press started within the double-tap window */
    TOUCH_EVENT_LONG_PRESS, /* held past threshold; fires once, while still held */
} touch_event_t;

typedef void (*touch_event_cb_t)(touch_event_t event, void *ctx);

/* Configure the GPIO and start the polling task. Call once from app_main. */
esp_err_t touch_init(void);

/* Register the event consumer (single subscriber; replaces any previous one).
 * Pass cb = NULL to unregister. */
esp_err_t touch_register_callback(touch_event_cb_t cb, void *ctx);

/* Instantaneous debounced state; usable with or without a callback. */
bool touch_is_pressed(void);

#ifdef __cplusplus
}
#endif
