// BOARD PINS

#ifndef BOARD_PINS_H
#define BOARD_PINS_H
 
// Power pins (not GPIOs, listed for reference only)
// 5V   -> VBUS
// GND  -> Ground
// 3V3  -> Regulated 3.3V output, up to 700mA
 
// Digital / Analog / Touch pins
#define CAIPO_PIN_LED_DATA      1   // ADC, Touch1
#define D1_GPIO_NUM       2   // ADC, Touch2
#define D2_GPIO_NUM       3   // ADC, Touch3 -- strapping pin (JTAG signal source)
#define D3_GPIO_NUM       4   // ADC, Touch4
#define D4_GPIO_NUM       5   // I2C SDA, ADC, Touch5
#define D5_GPIO_NUM       6   // I2C SCL, ADC, Touch6
#define D6_GPIO_NUM       43  // UART TX
#define D7_GPIO_NUM       44  // UART RX
#define D8_GPIO_NUM       7   // SPI SCK, ADC, Touch7
#define D9_GPIO_NUM       8   // SPI MISO, ADC, Touch8
#define D10_GPIO_NUM      9   // SPI MOSI, ADC, Touch9
#define D11_GPIO_NUM      42  // ADC, Touch12 -- shared w/ mic DATA if Sense board active
#define D12_GPIO_NUM      41  // ADC, Touch13 -- shared w/ mic CLK if Sense board active
 
// JTAG pins (shared with camera SCCB / mic when Sense board is active)
#define MTDO_GPIO_NUM     40  // JTAG -- shared w/ camera SIOD
#define MTDI_GPIO_NUM     41  // JTAG, ADC -- shared w/ mic
#define MTCK_GPIO_NUM     39  // JTAG, ADC -- shared w/ camera SIOC
#define MTMS_GPIO_NUM     42  // JTAG, ADC -- shared w/ mic
 
// Control / system pins
#define RESET_PIN_NUM     -1  // CHIP_PU, not a standard GPIO
#define BOOT_GPIO_NUM     0   // Enter boot mode -- strapping pin
#define USER_LED_GPIO_NUM 21  // Onboard orange user LED
 
#endif // BOARD_PINS_H

// CAMERA PINS 

#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H
 
// Power / reset (Sense board camera has no dedicated PWDN/RESET GPIO)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
 
// Clock
#define XCLK_GPIO_NUM     10
 
// SCCB (camera config I2C — NOT the main I2C bus)
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
 
// Data bus (Y2-Y9)
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
 
// Sync / clock
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13
 
#endif // CAMERA_PINS_H