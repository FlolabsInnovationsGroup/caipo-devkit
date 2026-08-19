#include "touch_gesture.h"

/*
 * States:
 *   IDLE            — not pressed, no pending gesture
 *   PRESSED_FIRST   — first press active; may become TAP, DOUBLE_TAP start, or LONG_PRESS
 *   WAIT_SECOND     — released after a short press; waiting out the double-tap window
 *   PRESSED_AFTER_DT— press that already fired DOUBLE_TAP; only RELEASE can follow
 *   HOLD_CONSUMED   — LONG_PRESS fired; swallow the hold until release
 *
 * DOUBLE_TAP fires when the second press *starts* inside the window (lowest
 * latency, matches the plan in docs/component-steps.md). A press after LONG_PRESS
 * or DOUBLE_TAP resolves emits no further gesture, only RELEASE.
 */

enum {
    ST_IDLE,
    ST_PRESSED_FIRST,
    ST_WAIT_SECOND,
    ST_PRESSED_AFTER_DT,
    ST_HOLD_CONSUMED,
};

void touch_gesture_init(touch_gesture_sm_t *sm, const touch_gesture_cfg_t *cfg)
{
    sm->cfg = *cfg;
    sm->state = ST_IDLE;
    sm->t_press_ms = 0;
    sm->t_release_ms = 0;
}

int touch_gesture_feed(touch_gesture_sm_t *sm, bool pressed, uint32_t now_ms,
                       touch_gesture_event_t out[TOUCH_GESTURE_MAX_EVENTS])
{
    int n = 0;

    switch (sm->state) {
    case ST_IDLE:
        if (pressed) {
            sm->t_press_ms = now_ms;
            sm->state = ST_PRESSED_FIRST;
            out[n++] = TOUCH_GESTURE_PRESS;
        }
        break;

    case ST_PRESSED_FIRST:
        if (!pressed) {
            sm->t_release_ms = now_ms;
            sm->state = ST_WAIT_SECOND;
            out[n++] = TOUCH_GESTURE_RELEASE;
        } else if (now_ms - sm->t_press_ms >= sm->cfg.longpress_ms) {
            sm->state = ST_HOLD_CONSUMED;
            out[n++] = TOUCH_GESTURE_LONG_PRESS;
        }
        break;

    case ST_WAIT_SECOND:
        if (pressed) {
            if (now_ms - sm->t_release_ms <= sm->cfg.doubletap_window_ms) {
                sm->state = ST_PRESSED_AFTER_DT;
                out[n++] = TOUCH_GESTURE_PRESS;
                out[n++] = TOUCH_GESTURE_DOUBLE_TAP;
            } else {
                /* Window expired unnoticed (no poll landed in between):
                 * resolve the old gesture, then start the new one. */
                sm->t_press_ms = now_ms;
                sm->state = ST_PRESSED_FIRST;
                out[n++] = TOUCH_GESTURE_TAP;
                out[n++] = TOUCH_GESTURE_PRESS;
            }
        } else if (now_ms - sm->t_release_ms > sm->cfg.doubletap_window_ms) {
            sm->state = ST_IDLE;
            out[n++] = TOUCH_GESTURE_TAP;
        }
        break;

    case ST_PRESSED_AFTER_DT:
        if (!pressed) {
            sm->state = ST_IDLE;
            out[n++] = TOUCH_GESTURE_RELEASE;
        }
        break;

    case ST_HOLD_CONSUMED:
        if (!pressed) {
            sm->state = ST_IDLE;
            out[n++] = TOUCH_GESTURE_RELEASE;
        }
        break;
    }

    return n;
}

void touch_debounce_init(touch_debounce_t *db, int required_samples, bool initial)
{
    db->stable = initial;
    db->candidate = initial;
    db->count = 0;
    db->required = required_samples > 0 ? required_samples : 1;
}

bool touch_debounce_feed(touch_debounce_t *db, bool raw)
{
    if (raw == db->stable) {
        db->candidate = db->stable;
        db->count = 0;
    } else if (raw == db->candidate) {
        if (++db->count >= db->required) {
            db->stable = raw;
            db->count = 0;
        }
    } else {
        db->candidate = raw;
        db->count = 1;
        if (db->count >= db->required) {
            db->stable = raw;
            db->count = 0;
        }
    }
    return db->stable;
}
