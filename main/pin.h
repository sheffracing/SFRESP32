#ifndef SFR_PIN
#define SFR_PIN

/* All Devices */
#define SFR_TAG  "SFR_ESP32"

#define GPIO_ONBOARD_LED 15
#define GPIO_CAN0_TX 22
#define GPIO_CAN0_RX 23

/* CAN bus 2 optional */
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
#define EVE_PDN 16
#define EVE_CS 17

/* IMD Monitor Only */
#define IMD_PWM_IN ADC_CHANNEL_0 // GPIO0

/* APPs Only */
#define APPS1_IN ADC_CHANNEL_0 // GPIO0
#define APPS2_IN ADC_CHANNEL_1 // GPIO1

/* Contactor Drive Only */
#define GPIO_PRECHARGE_CONTACTOR_PWM 0 
#define GPIO_MAIN_POS_CONTACTOR_PWM 0
#define GPIO_MAIN_NEG_CONTACTOR_PWM 0
#define ADC_PRECHARGE_CAR_SIDE ADC_CHANNEL_0 // GPIO0
#define ADC_PRECHARGE_ACCU_SIDE ADC_CHANNEL_1 // GPIO1
#define ADC_MAIN_POS_CAR_SIDE ADC_CHANNEL_2 // GPIO2
#define ADC_MAIN_NEG_CAR_SIDE ADC_CHANNEL_3 // GPIO3
#define ADC_MAIN_POS_ACCU_SIDE ADC_CHANNEL_4 // GPIO4
#define ADC_MAIN_NEG_ACCU_SIDE ADC_CHANNEL_6 // GPIO6

/* TempMonitor Only */
#define SPI_MCP3208_CS 17

/* Dyno Only */
#define SPI_MCP3204_1_CS 17
#define SPI_MCP3204_2_CS 16

/* PDU Only */
#define SPI_MCP3208_CS 17
#define CONTROL_RADFAN 0 // Extra 1
#define CONTROL_HORN 1 // Extra 2
#define CONTROL_PUMP 2 // Pump 1 and 2
#define CONTROL_ACCUFAN 21 // Fan 1 and 2
#define CONTROL_RELAY 16 // Relay

#endif // SFR_PIN
