#ifndef SFR_PIN
#define SFR_PIN

#define SFR_TAG  "SFR_BMS"

/* SDIO - SD Card connection */
#define SDIO_CLK    19
#define SDIO_CMD    18
#define SDIO_D0     20
#define SDIO_D1     21
#define SDIO_D2     22
#define SDIO_D3     23

/* SPI Bus 2 */
#define SPI2_CLK    6
#define SPI2_CS     16
#define SPI2_MISO   7
#define SPI2_MOSI   2

/* CAN */
#define GPIO_CAN0_TX     0
#define GPIO_CAN0_RX     1

/* LEDs */
#define LED1        10 //Red
#define LED2        11 //Green

#endif // SFR_PIN
