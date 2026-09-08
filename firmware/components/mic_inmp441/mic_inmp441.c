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

#include "mic_inmp441.h"
#include "caipo_pins.h"


extern QueueHandle_t audio_queue; // External queue for audio data transfer
i2s_chan_handle_t rx_chan; // I2S rx channel handler
i2s_chan_handle_t tx_chan;


void i2s_read_task(void *args)
{
    uint8_t *r_buf = (uint8_t *)calloc(1, BUFF_SIZE);
    assert(r_buf); // Check if r_buf allocation success
    size_t r_bytes = 0;

    // Enable the RX channel
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));

    while (1) {
        // Read i2s data
        i2s_channel_read(rx_chan, r_buf, BUFF_SIZE, &r_bytes, 1000);
        //xQueueSend(audio_queue, &r_buf, pdMS_TO_TICKS(100));
        
        // Send data to the audio queue
        if (r_bytes > 0 && audio_queue != NULL) {
            uint8_t *tx_buf = (uint8_t *)calloc(1, BUFF_SIZE);
            if (tx_buf != NULL) {
                memcpy(tx_buf, r_buf, r_bytes);
                if (xQueueSend(audio_queue, &tx_buf, pdMS_TO_TICKS(100)) != pdPASS) {
                    free(tx_buf);
                }
            }
        }
    }

    i2s_channel_disable(rx_chan);
    free(r_buf);
    vTaskDelete(NULL);
}

void i2s_mic_init_std(void)
{
    i2s_chan_config_t rx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    rx_chan_cfg.dma_frame_num = 255;
    ESP_ERROR_CHECK(i2s_new_channel(&rx_chan_cfg, &tx_chan, &rx_chan));

    i2s_std_config_t rx_std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(CLOCK_SPEED),
        .clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384,
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_24BIT, I2S_SLOT_MODE_MONO),
        .slot_cfg.slot_mask = I2S_STD_SLOT_LEFT,
        .gpio_cfg = {
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .din  = I2S_MICROPHONE_DIN,
            .dout = I2S_SPEAKER_DOUT,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &rx_std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &rx_std_cfg));
}

void mic_main(void)
{
    i2s_mic_init_std();

    // Create reading task, enable and start the channel
    xTaskCreate(i2s_read_task, "i2s_read_task", 4096, NULL, 5, NULL);
}