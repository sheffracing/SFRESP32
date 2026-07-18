/*
main.c | CAN Blaster | ESP32-C6 | Sheffield Formula Racing
File contains the main function for the ESP32-C6 based CAN Blaster.
This program sends set can messages out to the bus to test loading and other devicess recieve functionality.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

/* --------------------------- Includes ----------------------------- */

#include <stdio.h>
#include <stdlib.h>

#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "CAN/can.h"
#include "tasks.h"
#include "pin.h"
#include "espnow.h"
#include "sdcard.h"
#include "adc.h"
#include "I2C.h"
#include "CAN/canflash.h"
#include "NVHDisplay.h"
#include "mcp320X.h"

/* --------------------------- Definitions ----------------------------- */
#define TIMER_INTERVAL_WD       100     // in microseconds
#define TIMER_INTERVAL_GP       100
#define INTERRUPT_INTERVAL_1MS   1000    // in microseconds
#define INTERRUPT_INTERVAL_100MS 100000 
#define TIMER_INTERVAL_1MS      pdMS_TO_TICKS(1)       // in milliseconds
#define TIMER_INTERVAL_100MS    pdMS_TO_TICKS(100)

/* --------------------------- Global Variables ----------------------------- */
esp_timer_handle_t stTaskInterrupt1ms;
esp_timer_handle_t stTaskInterrupt100ms;
esp_reset_reason_t eResetReason;
eChipMode_t eDeviceMode = eNORMAL;
spi_device_handle_t MCP320XDevs[2];

/* --------------------------- Function prototypes ----------------------------- */
static void timers_init(void);
static void main_init(void);
static void GPIO_init(void);
void call_back_1ms(void *arg);
void call_back_100ms(void *arg);

/* --------------------------- Functions ----------------------------- */

void app_main(void)
{
    /* Get last reset reason */
    eResetReason = esp_reset_reason();
    
    /* Register app_main task with the WDT */ 
    (void)esp_task_wdt_deinit(); 
    esp_task_wdt_config_t stWDTConfig = {
        .timeout_ms = 2000, 
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_task_wdt_init(&stWDTConfig);
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL)); 

    /* Initialise device */
    main_init();

    /* Run background task all other tasks are called by interrupts */
    while(1)
    {
        if (eDeviceMode == eNORMAL)
        {
            task_BG();
        } else
        {
            reflash_task_BG();
        }
    }
}

void IRAM_ATTR call_back_1ms(void *arg)
{
    if (eDeviceMode == eNORMAL)
    {
        task_1ms();
    } 
}

void IRAM_ATTR call_back_100ms(void *arg)
{
    if (eDeviceMode == eNORMAL)
    {
        task_100ms();
    } else
    {
        reflash_task_100ms();
    }
}

static void main_init(void)
{
    /* Initialises Features/ Peripherals, Comment out as needed*/

    /* ESP-NOW */
    // eStatus = ESPNOW_init();
    // if (eStatus != ESP_OK)
    // {
    //     ESP_LOGE(SFR_TAG, "Failed to initialise ESP-NOW: %s", esp_err_to_name(eStatus));
    // }

    /* SD Card (SDCard and LCD share the SPI bus, take care) */
    /* SPI Devices */
    // spi_bus_config_t stBusConfig = 
    // {
    //     .mosi_io_num = SPI_MOSI,
    //     .miso_io_num = SPI_MISO,
    //     .sclk_io_num = SPI_SCK,
    //     .quadwp_io_num = -1,
    //     .quadhd_io_num = -1,
    // };
    // eStatus = spi_bus_initialize(SPI2_HOST, &stBusConfig, SPI_DMA_CH_AUTO);

    /* SD Card */
    // eStatus = SD_card_init();
    // if (eStatus != ESP_OK)
    // {
    //     ESP_LOGE(SFR_TAG, "Failed to initialise SD Card: %s", esp_err_to_name(eStatus));
    // }

    /* ADC MCP3204/8 This is a example config is required */
    // uint8_t aNCSPins[2] = {SPI_MCP3204_1_CS, SPI_MCP3204_2_CS};
    // eStatus = MCP320X_init(aNCSPins, MCP320XDevs);
    // if (eStatus != ESP_OK)
    // {
    //     ESP_LOGE(SFR_TAG, "Failed to initialise MCP320X: %s", esp_err_to_name(eStatus));
    // }
    /* END of SPI Devices*/
    
    /* CAN BUS */
    // NStatus = CAN_init(TRUE);
    // if (NStatus != ESP_OK)
    // {
    //     ESP_LOGE(SFR_TAG, "Failed to initialise CAN: %s", esp_err_to_name(NStatus));
    // }
    // NStatus = CAN_flash_init();
    // if (NStatus != ESP_OK)
    // {
    //     ESP_LOGE(SFR_TAG, "Failed to initialise CAN Reflash: %s", esp_err_to_name(NStatus));
    // }

    /* LDC (SDCard and LCD share the SPI bus, take care) */
    NVHDisplay_init();


    /* External Clock */
    // eStatus = I2C_init();
    // if (eStatus != ESP_OK)
    // {
    //     ESP_LOGE(SFR_TAG, "Failed to initialise I2C: %s", esp_err_to_name(eStatus));
    // }

    /* ADC */
    
    /* Timers and GPIO cause a hard fault on fail so no error warning */
    GPIO_init();
    timers_init();  
    
}

static void timers_init(void)
{
    const esp_timer_create_args_t stTaskInterrupt1msArgs = {
        .callback = &call_back_1ms,
        .arg = NULL,
        .name = "1ms"
    };
    const esp_timer_create_args_t stTaskInterrupt100msArgs = {
        .callback = &call_back_100ms,
        .arg = NULL,
        .name = "100ms"
    };

    ESP_ERROR_CHECK(esp_timer_create(&stTaskInterrupt1msArgs, &stTaskInterrupt1ms));
    ESP_ERROR_CHECK(esp_timer_start_periodic(stTaskInterrupt1ms, INTERRUPT_INTERVAL_1MS)); // 1ms
    ESP_ERROR_CHECK(esp_timer_create(&stTaskInterrupt100msArgs, &stTaskInterrupt100ms));
    ESP_ERROR_CHECK(esp_timer_start_periodic(stTaskInterrupt100ms, INTERRUPT_INTERVAL_100MS)); // 100ms
}

static void GPIO_init(void)
{
    gpio_config_t onboardLEDConfig = {
        .pin_bit_mask = 1ULL << GPIO_ONBOARD_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&onboardLEDConfig);
}

void set_device_mode(eChipMode_t mode)
{
    eDeviceMode = mode;
}

