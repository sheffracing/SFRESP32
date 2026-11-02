#ifndef MCP3208_H
#define MCP3208_H

#include "pin.h"
#include "sfrtypes.h"
#include "driver/spi_master.h"

#include "string.h"
#include "esp_err.h"
#include "esp_log.h"

/* --------------------------- Function prototypes -------------------- */
esp_err_t MCP320X_init(uint8_t NCSPin, spi_device_handle_t stDeviceHandle);
float MCP320X_read(spi_device_handle_t stDeviceHandle, uint8_t NDevADC);

#endif