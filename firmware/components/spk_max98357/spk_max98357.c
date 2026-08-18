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

i2s_chan_handle_t tx_chan; // I2S tx channel handler


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
            if (rx_buf != NULL) {
                // Write the received audio data
                i2s_channel_write(tx_chan, rx_buf, BUFF_SIZE, &w_bytes, 1000);
                
                // Free the received buffer
                free(rx_buf);
                rx_buf = NULL;
            }
        } else {
            // No data received, write silence
            memset(w_buf, 0, BUFF_SIZE);
            i2s_channel_write(tx_chan, w_buf, BUFF_SIZE, &w_bytes, 1000);
        }
    }

    free(w_buf);
    vTaskDelete(NULL);
}

void i2s_init_std(void)
{
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));

    i2s_std_config_t tx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(48000),
        .clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384, // Is 384 when using 24-bit data width
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_MONO),
        .slot_cfg.slot_mask = I2S_STD_SLOT_LEFT,
        .gpio_cfg = {
            .bclk = I2S_SPEAKER_BCLK,
            .ws   = I2S_SPEAKER_WS,
            .dout = I2S_SPEAKER_DOUT,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
}

void spk_main(void)
{
    i2s_init_std();

    // Create writing task, enable and start the channel
    xTaskCreate(i2s_write_task, "i2s_example_write_task", 4096, NULL, 5, NULL);
}