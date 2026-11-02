#ifndef SDCARD

#include <unistd.h>
#include <stdio.h>
#include "pin.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "espnow.h"
#include "main.h"
#include "dirent.h"
#include "CAN/can.h"

esp_err_t SD_card_init(void);
esp_err_t sdcard_empty_buffer(void);
esp_err_t SD_card_write(byte *abyData);
esp_err_t SD_card_write_CAN(CAN_frame_t stCANFrame, dword dwTimestamp);
esp_err_t sdcard_log_imu(void);
esp_err_t sdcard_log_time(void);

#define SDCARD
#endif