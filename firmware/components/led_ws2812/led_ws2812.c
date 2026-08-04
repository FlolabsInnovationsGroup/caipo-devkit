#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "led_ws2812.h"
#include "caipo_pins.h"

// Use RMT or SPI to control the LEDs
#define BLINK_LED_STRIP_BACKEND_RMT 1
// #define BLINK_LED_STRIP_BACKEND_SPI 1

static const char *TAG = "led_ws2812";


static uint8_t s_led_state = 0;

#ifdef CAIPO_PIN_LED_DATA

static led_strip_handle_t led_strip;

void ctrl_led(led_strip_handle_t led_strip, uint8_t num_led, uint8_t col1, uint8_t col2, uint8_t col3, uint8_t w)
{
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    led_strip_set_pixel_rgbw(led_strip, num_led, col1, col2, col3, w);
}

void configure_led(void)
{
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = CAIPO_PIN_LED_DATA,
        .max_leds = CAIPO_LED_COUNT,
        .led_model = LED_MODEL,
        .color_component_format = COLOR_COMPONENT_FORMAT,

    };
#if BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = RMT_RESOLUTON_HZ,
        .flags.with_dma = FLAG_WITH_DMA,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = FLAG_WITH_DMA,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);

    ESP_LOGI(TAG, "Program configured to blink addressable LED!");
}

#else
#error "no LED pin registered"
#endif

void blink(void)
{

    /* Configure the peripheral according to the LED type */
    configure_led();

    while (1) {
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        if (s_led_state) {
            for(uint8_t i = 0; i < CAIPO_LED_COUNT; i++){
                /* Set the LED pixel using RGBW from 0 (0%) to 255 (100%) for each color */
                ctrl_led(led_strip, i, 4, 0, 0, 0);
            }
            /* Refresh the strip to send data */
            led_strip_refresh(led_strip);
        } else {
            /* Set all LED off to clear all pixels */
            led_strip_clear(led_strip);
        }
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(1000 / 10);
    }
}
