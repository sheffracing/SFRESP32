#ifndef CANflashH

#define CANflashH
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "./../main.h"
#include "can.h"

esp_err_t CAN_flash_empty_queue(esp_partition_t *stOTAPartition);
esp_err_t CAN_flash_write(esp_partition_t *stOTAPartition);
uint32_t CAN_flash_get_size();
esp_err_t CAN_flash_init();
uint16_t crc8(uint64_t NData);

extern uint32_t NBytesWrittenReflash;
extern uint16_t NErrorCountReflash;
extern uint32_t NFirmwareSize;

#endif