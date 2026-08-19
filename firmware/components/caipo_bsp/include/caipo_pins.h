#pragma once

#include "driver/gpio.h"

/*
 * Caipo DEV KIT v0 pin map — Seeed Studio XIAO ESP32-S3.
 *
 * Source: CAIPO DEV KIT v0 doc (FLD-1), Appendix A. The doc's "GPIO" column
 * appears to number the XIAO Dx pads, not the ESP32-S3 chip GPIOs; chip GPIOs
 * below use the XIAO silkscreen mapping (D0..D10 = GPIO 1,2,3,4,5,6,43,44,7,8,9).
 * Each pin must be verified on hardware before its driver is trusted.
 */

/* Touch sensor: TTP223 OUT, active high, momentary mode. Doc: "D0".
 * Chip GPIO0 is a boot strapping pin, which supports the Dx-index reading. */
#define CAIPO_PIN_TOUCH GPIO_NUM_1

/* TODO(verify on hardware, then define as drivers are implemented):
 *   RGB LED (WS2812B/SK6812 DIN)  doc "D1"  -> GPIO2
 *   Pi power enable (S13V30F5 EN) doc "D2"  -> GPIO3
 *   UART RX (from Pi TX)          doc "D3"  -> GPIO4
 *   UART TX (to Pi RX)            doc "D4"  -> GPIO5
 *   Battery ADC (2:1 divider)     doc "D5"  -> GPIO6
 *   I2C SDA                       doc "D6"  -> GPIO43 (doc §4.2 says GPIO6 — conflict)
 *   I2C SCL                       doc "D7"  -> GPIO44 (doc §4.2 says GPIO7 — conflict)
 *   I2S SD (mic INMP441)          doc "D8"  -> GPIO7
 *   I2S SCK (mic + amp, shared)   doc "D9"  -> GPIO8
 *   I2S WS  (mic + amp, shared)   doc "D10" -> GPIO9
 *   I2S DIN (amp MAX98357A)       doc "A0"  -> A0 aliases D0 on the XIAO; conflicts with touch — unresolved
 */
