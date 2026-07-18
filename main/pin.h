#ifndef SFR_PIN
#define SFR_PIN

/* All Devices */
#define SFR_TAG  "SFR_ESP32"

#define GPIO_ONBOARD_LED 15

// for Screen PCB, the RX and TX pins are different 
#define GPIO_CAN0_TX 16
#define GPIO_CAN0_RX 17

// #define GPIO_CAN1_TX XX
// #define GPIO_CAN1_RX XX

/* SPI Common For all devices */
#define SPI_MOSI 18
#define SPI_SCK 19
#define SPI_MISO 20

/* Logger only */
#define SPI_SD_CS 17
#define I2C0_SCL 2
#define I2C0_SDA 21

/* Wheel Only */
#define EVE_PDN 0
#define EVE_CS 1

/* IMD Monitor Only */
#define IMD_PWM_IN ADC_CHANNEL_0 // GPIO0

/* APPs Only */
#define APPS1_IN ADC_CHANNEL_0 // GPIO0
#define APPS2_IN ADC_CHANNEL_1 // GPIO1

/* Contactor Drive Only */
#define GPIO_PRECHARGE_CONTACTOR_PWM 0 
#define GPIO_MAIN_POS_CONTACTOR_PWM 1
#define GPIO_MAIN_NEG_CONTACTOR_PWM 2

/* TempMonitor Only */
#define SPI_MCP3208_CS 17

/* Dyno Only */
#define SPI_MCP3204_1_CS 17
#define SPI_MCP3204_2_CS 16

#endif // SFR_PIN