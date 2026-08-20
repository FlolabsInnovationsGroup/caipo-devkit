#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "led_ws2812.h"
#include "mic_inmp441.h"
#include "spk_max98357.h"
#include "touch_ttp223.h"

static const char *TAG = "main";

QueueHandle_t audio_queue = NULL; // Global queue for audio transfer from microphone to speaker

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
    // Create queue for audio data transfer (queue holds pointer to audio buffer)
    audio_queue = xQueueCreate(1, BUFF_SIZE);
    if (audio_queue == NULL) {
        printf("Failed to create audio queue\n");
        return;
    }

    // LEDs test code
    //blink();

    // Microphone and speaker test code
    mic_main();   
    spk_main();

    ESP_ERROR_CHECK(touch_init());
    ESP_ERROR_CHECK(touch_register_callback(on_touch, NULL));
}
