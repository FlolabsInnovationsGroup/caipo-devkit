#include "led_strip.h"

// Defines
#define CAIPO_LED_COUNT 12 // Number of LEDs
#define LED_MODEL LED_MODEL_WS2812 // LED model
#define COLOR_COMPONENT_FORMAT LED_STRIP_COLOR_COMPONENT_FMT_GRBW // LED strip colour format

#define RMT_RESOLUTON_HZ 10 * 1000 * 1000 // 10MHz
#define FLAG_WITH_DMA true // Use DMA?

// Functions declaration
void blink(void); // Blink program
void ctrl_led(led_strip_handle_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t); // Control one LED
void configure_led(void); // Configure LED strip