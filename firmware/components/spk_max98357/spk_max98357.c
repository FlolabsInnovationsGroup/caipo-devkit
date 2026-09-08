#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"

#include "spk_max98357.h"
#include "caipo_pins.h"

// External queue for audio data transfer
extern QueueHandle_t audio_queue;

extern i2s_chan_handle_t tx_chan; // Extern I2S tx channel handler


void i2s_write_task(void *args)
{
    uint8_t *w_buf = (uint8_t *)calloc(1, BUFF_SIZE);
    assert(w_buf); // Check if w_buf allocation success
    size_t w_bytes = BUFF_SIZE;
    uint8_t *rx_buf = NULL;

    // Enable the TX channel
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    while (1) {
        // Try to receive audio data from the queue
        if (xQueueReceive(audio_queue, &rx_buf, pdMS_TO_TICKS(1000)) == pdPASS) {
            i2s_channel_write(tx_chan, rx_buf, BUFF_SIZE, &w_bytes, 1000);
            free(rx_buf);
            rx_buf = NULL;
        } else {
            // No data received, write silence
            memset(w_buf, 0, BUFF_SIZE);
            i2s_channel_write(tx_chan, w_buf, BUFF_SIZE, &w_bytes, 1000);
        }
    }

    free(w_buf);
    vTaskDelete(NULL);
}

void spk_main(void)
{
    //i2s_init_std();

    // Create writing task, enable and start the channel
    xTaskCreate(i2s_write_task, "i2s_example_write_task", 4096, NULL, 5, NULL);
}