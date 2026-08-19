#include "esp_log.h"
#include "touch_ttp223.h"

static const char *TAG = "main";

static void on_touch(touch_event_t event, void *ctx)
{
    (void)ctx;
    static const char *names[] = {
        "PRESS", "RELEASE", "TAP", "DOUBLE_TAP", "LONG_PRESS",
    };
    ESP_LOGI(TAG, "touch: %s", names[event]);
}

void app_main(void)
{
    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(touch_register_callback(on_touch, NULL));
}
