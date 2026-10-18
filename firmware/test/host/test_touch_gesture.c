/* Host-side unit test for the touch gesture classifier (no ESP-IDF).
 * Feeds synthetic (pressed, timestamp) timelines and checks emitted events. */

#include <stdio.h>
#include <string.h>

#include "touch_gesture.h"

#define LONGPRESS_MS 2000
#define WINDOW_MS 300
#define POLL_MS 50

static const char *EV_NAMES[] = {
    "PRESS", "RELEASE", "TAP", "DOUBLE_TAP", "LONG_PRESS",
};

static int g_failures;

typedef struct {
    touch_gesture_sm_t sm;
    touch_gesture_event_t log[32];
    int count;
} harness_t;

static void harness_init(harness_t *h)
{
    const touch_gesture_cfg_t cfg = {
        .longpress_ms = LONGPRESS_MS,
        .doubletap_window_ms = WINDOW_MS,
    };
    touch_gesture_init(&h->sm, &cfg);
    h->count = 0;
}

/* Feed samples every POLL_MS from t_start to t_end inclusive. */
static void feed_span(harness_t *h, bool pressed, uint32_t t_start, uint32_t t_end)
{
    for (uint32_t t = t_start; t <= t_end; t += POLL_MS) {
        touch_gesture_event_t out[TOUCH_GESTURE_MAX_EVENTS];
        int n = touch_gesture_feed(&h->sm, pressed, t, out);
        for (int i = 0; i < n && h->count < 32; i++) {
            h->log[h->count++] = out[i];
        }
    }
}

static void expect(const char *test, harness_t *h,
                   const touch_gesture_event_t *want, int want_count)
{
    int ok = h->count == want_count;
    for (int i = 0; ok && i < want_count; i++) {
        ok = h->log[i] == want[i];
    }
    if (ok) {
        printf("PASS  %s\n", test);
        return;
    }
    g_failures++;
    printf("FAIL  %s\n  want:", test);
    for (int i = 0; i < want_count; i++) printf(" %s", EV_NAMES[want[i]]);
    printf("\n  got: ");
    for (int i = 0; i < h->count; i++) printf(" %s", EV_NAMES[h->log[i]]);
    printf("\n");
}

static void test_clean_tap(void)
{
    harness_t h;
    harness_init(&h);
    feed_span(&h, false, 0, 950);      /* idle */
    feed_span(&h, true, 1000, 1100);   /* short press */
    feed_span(&h, false, 1150, 1600);  /* release, wait out the window */
    const touch_gesture_event_t want[] = {
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_RELEASE, TOUCH_GESTURE_TAP,
    };
    expect("clean tap", &h, want, 3);
}

static void test_long_press(void)
{
    harness_t h;
    harness_init(&h);
    feed_span(&h, true, 0, 2100);      /* hold past threshold */
    feed_span(&h, false, 2150, 2600);  /* release; must NOT add a TAP */
    const touch_gesture_event_t want[] = {
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_LONG_PRESS, TOUCH_GESTURE_RELEASE,
    };
    expect("long press (fires while held, no trailing tap)", &h, want, 3);
}

static void test_double_tap(void)
{
    harness_t h;
    harness_init(&h);
    feed_span(&h, true, 0, 100);    /* first press */
    feed_span(&h, false, 150, 200); /* short release */
    feed_span(&h, true, 250, 350);  /* second press within window */
    feed_span(&h, false, 400, 900); /* release; no TAP afterwards */
    const touch_gesture_event_t want[] = {
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_RELEASE,
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_DOUBLE_TAP,
        TOUCH_GESTURE_RELEASE,
    };
    expect("double tap", &h, want, 5);
}

static void test_two_slow_taps(void)
{
    harness_t h;
    harness_init(&h);
    feed_span(&h, true, 0, 100);      /* first tap */
    feed_span(&h, false, 150, 550);   /* window (300 ms) expires -> TAP */
    feed_span(&h, true, 600, 700);    /* second, separate tap */
    feed_span(&h, false, 750, 1200);  /* -> its own TAP */
    const touch_gesture_event_t want[] = {
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_RELEASE, TOUCH_GESTURE_TAP,
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_RELEASE, TOUCH_GESTURE_TAP,
    };
    expect("two slow taps", &h, want, 6);
}

static void test_missed_window_poll(void)
{
    /* Second press arrives after the window, and no poll landed between the
     * window expiry and that press: the TAP and the new PRESS must both come
     * out of the same feed call. */
    harness_t h;
    harness_init(&h);
    feed_span(&h, true, 0, 100);
    feed_span(&h, false, 150, 400);  /* last idle poll still inside window (150+300=450) */
    feed_span(&h, true, 500, 550);   /* first poll after expiry is already pressed */
    feed_span(&h, false, 600, 1000);
    const touch_gesture_event_t want[] = {
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_RELEASE,
        TOUCH_GESTURE_TAP, TOUCH_GESTURE_PRESS,
        TOUCH_GESTURE_RELEASE, TOUCH_GESTURE_TAP,
    };
    expect("late press resolves old tap + starts new gesture", &h, want, 6);
}

static void test_hold_after_double_tap(void)
{
    /* press-release-press held long: DOUBLE_TAP already fired, the long hold
     * must not additionally fire LONG_PRESS. */
    harness_t h;
    harness_init(&h);
    feed_span(&h, true, 0, 100);
    feed_span(&h, false, 150, 200);
    feed_span(&h, true, 250, 3000);   /* way past longpress threshold */
    feed_span(&h, false, 3050, 3500);
    const touch_gesture_event_t want[] = {
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_RELEASE,
        TOUCH_GESTURE_PRESS, TOUCH_GESTURE_DOUBLE_TAP,
        TOUCH_GESTURE_RELEASE,
    };
    expect("hold after double tap adds no long press", &h, want, 5);
}

static void test_debounce(void)
{
    touch_debounce_t db;
    touch_debounce_init(&db, 2, false);

    int ok = 1;
    ok &= touch_debounce_feed(&db, false) == false;
    ok &= touch_debounce_feed(&db, true) == false;  /* 1st sample: not yet */
    ok &= touch_debounce_feed(&db, false) == false; /* glitch resets */
    ok &= touch_debounce_feed(&db, true) == false;
    ok &= touch_debounce_feed(&db, true) == true;   /* 2 consecutive: flips */
    ok &= touch_debounce_feed(&db, false) == true;  /* single dropout ignored */
    ok &= touch_debounce_feed(&db, true) == true;
    ok &= touch_debounce_feed(&db, false) == true;
    ok &= touch_debounce_feed(&db, false) == false; /* 2 consecutive: flips back */

    if (ok) {
        printf("PASS  debounce (2-sample)\n");
    } else {
        g_failures++;
        printf("FAIL  debounce (2-sample)\n");
    }
}

int main(void)
{
    test_clean_tap();
    test_long_press();
    test_double_tap();
    test_two_slow_taps();
    test_missed_window_poll();
    test_hold_after_double_tap();
    test_debounce();

    printf(g_failures ? "\n%d test(s) FAILED\n" : "\nall tests passed\n", g_failures);
    return g_failures ? 1 : 0;
}
