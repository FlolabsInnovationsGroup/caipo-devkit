// BOARD PINS

#ifndef BOARD_PINS_H
#define BOARD_PINS_H
 
// Power pins (not GPIOs, listed for reference only)
// 5V   -> VBUS
// GND  -> Ground
// 3V3  -> Regulated 3.3V output, up to 700mA
 
// Digital / Analog / Touch pins
#define CAIPO_PIN_LED_DATA          GPIO_NUM_1   // Data pin for the LED strip
#define I2S_MICROPHONE_BCLK         GPIO_NUM_2   // Clock pin for the microphone
#define I2S_MICROPHONE_WS           GPIO_NUM_3   // Word Select pin for the microphone
#define I2S_MICROPHONE_DIN          GPIO_NUM_4   // Data pin for the microphone
#define I2S_SPEAKER_BCLK            GPIO_NUM_5   // Clock pin for the speaker
#define I2S_SPEAKER_WS              GPIO_NUM_6   // Word Select pin for the speaker
#define I2S_SPEAKER_DOUT            GPIO_NUM_7   // Data pin for the speaker
#define D9_GPIO_NUM                 GPIO_NUM_8   // SPI MISO, ADC, Touch8
#define D10_GPIO_NUM                GPIO_NUM_9   // SPI MOSI, ADC, Touch9
#define D12_GPIO_NUM                GPIO_NUM_41  // ADC, Touch13 -- shared w/ mic CLK if Sense board active
#define D11_GPIO_NUM                GPIO_NUM_42  // ADC, Touch12 -- shared w/ mic DATA if Sense board active
#define D6_GPIO_NUM                 GPIO_NUM_43  // UART TX
#define D7_GPIO_NUM                 GPIO_NUM_44  // UART RX
 
// JTAG pins (shared with camera SCCB / mic when Sense board is active)
#define MTDO_GPIO_NUM     GPIO_NUM_40  // JTAG -- shared w/ camera SIOD
#define MTDI_GPIO_NUM     GPIO_NUM_41  // JTAG, ADC -- shared w/ mic
#define MTCK_GPIO_NUM     GPIO_NUM_39  // JTAG, ADC -- shared w/ camera SIOC
#define MTMS_GPIO_NUM     GPIO_NUM_42  // JTAG, ADC -- shared w/ mic
 
// Control / system pins
#define RESET_PIN_NUM     -1  // CHIP_PU, not a standard GPIO
#define BOOT_GPIO_NUM     GPIO_NUM_0   // Enter boot mode -- strapping pin
#define USER_LED_GPIO_NUM GPIO_NUM_21  // Onboard orange user LED
 
#endif // BOARD_PINS_H

// CAMERA PINS 

#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H
 
// Power / reset (Sense board camera has no dedicated PWDN/RESET GPIO)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
 
// Clock
#define XCLK_GPIO_NUM     GPIO_NUM_10
 
// SCCB (camera config I2C — NOT the main I2C bus)
#define SIOD_GPIO_NUM     GPIO_NUM_40
#define SIOC_GPIO_NUM     GPIO_NUM_39
 
// Data bus (Y2-Y9)
#define Y9_GPIO_NUM       GPIO_NUM_48
#define Y8_GPIO_NUM       GPIO_NUM_11
#define Y7_GPIO_NUM       GPIO_NUM_12
#define Y6_GPIO_NUM       GPIO_NUM_14
#define Y5_GPIO_NUM       GPIO_NUM_16
#define Y4_GPIO_NUM       GPIO_NUM_18
#define Y3_GPIO_NUM       GPIO_NUM_17
#define Y2_GPIO_NUM       GPIO_NUM_15
 
// Sync / clock
#define VSYNC_GPIO_NUM    GPIO_NUM_38
#define HREF_GPIO_NUM     GPIO_NUM_47
#define PCLK_GPIO_NUM     GPIO_NUM_13
 
#endif // CAMERA_PINS_H