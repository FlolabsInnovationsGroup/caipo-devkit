#include "touch_ttp223.h"

#include "touch_gesture.h"
#include "caipo_pins.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "touch_ttp223";

static touch_gesture_sm_t s_sm;
static touch_debounce_t s_db;
static touch_event_cb_t s_cb;
static void *s_cb_ctx;
static volatile bool s_pressed;
static bool s_inited;

static touch_event_t to_public(touch_gesture_event_t ev)
{
    switch (ev) {
    case TOUCH_GESTURE_PRESS:      return TOUCH_EVENT_PRESS;
    case TOUCH_GESTURE_RELEASE:    return TOUCH_EVENT_RELEASE;
    case TOUCH_GESTURE_TAP:        return TOUCH_EVENT_TAP;
    case TOUCH_GESTURE_DOUBLE_TAP: return TOUCH_EVENT_DOUBLE_TAP;
    default:                       return TOUCH_EVENT_LONG_PRESS;
    }
}

static void touch_task(void *arg)
{
    (void)arg;
    for (;;) {
        bool raw = gpio_get_level(CAIPO_PIN_TOUCH) != 0; /* TTP223 OUT is active high */
        bool debounced = touch_debounce_feed(&s_db, raw);
        s_pressed = debounced;

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        touch_gesture_event_t events[TOUCH_GESTURE_MAX_EVENTS];
        int n = touch_gesture_feed(&s_sm, debounced, now_ms, events);

        /* TTP223 self-calibrates after power-up; its output is meaningless
         * until then, so drop events from the blanking window. */
        if (now_ms >= CONFIG_CAIPO_TOUCH_STARTUP_BLANK_MS) {
            for (int i = 0; i < n; i++) {
                ESP_LOGD(TAG, "event %d", events[i]);
                touch_event_cb_t cb = s_cb;
                if (cb) {
                    cb(to_public(events[i]), s_cb_ctx);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(CONFIG_CAIPO_TOUCH_POLL_MS));
    }
}

esp_err_t touch_init(void)
{
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    /* TTP223 output is push-pull: no pull resistor needed. Polled, no interrupt. */
    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << CAIPO_PIN_TOUCH,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io);
    if (err != ESP_OK) {
        return err;
    }

    /* Debounce: require the new level for this long, i.e. ceil(debounce/poll)
     * consecutive polls, but at least 2 so a single glitched sample never flips. */
    int required = (CONFIG_CAIPO_TOUCH_DEBOUNCE_MS + CONFIG_CAIPO_TOUCH_POLL_MS - 1)
                   / CONFIG_CAIPO_TOUCH_POLL_MS;
    if (required < 2) {
        required = 2;
    }
    touch_debounce_init(&s_db, required, false);

    const touch_gesture_cfg_t cfg = {
        .longpress_ms = CONFIG_CAIPO_TOUCH_LONGPRESS_MS,
        .doubletap_window_ms = CONFIG_CAIPO_TOUCH_DOUBLETAP_WINDOW_MS,
    };
    touch_gesture_init(&s_sm, &cfg);

    if (xTaskCreate(touch_task, "touch", 2048, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_inited = true;
    ESP_LOGI(TAG, "polling GPIO%d every %d ms", CAIPO_PIN_TOUCH, CONFIG_CAIPO_TOUCH_POLL_MS);
    return ESP_OK;
}

esp_err_t touch_register_callback(touch_event_cb_t cb, void *ctx)
{
    s_cb_ctx = ctx;
    s_cb = cb;
    return ESP_OK;
}

bool touch_is_pressed(void)
{
    return s_pressed;
}
