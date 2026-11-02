#ifndef SFRTIMER

#include "main.h"

#include "driver/i2c_master.h"

#include "pin.h"

/* --------------------------- Function prototypes -------------------------- */
esp_err_t I2C_init(void);
esp_err_t I2C_write(i2c_master_dev_handle_t dev_handle, uint8_t *abyData, size_t NDataLength);
esp_err_t I2C_read(i2c_master_dev_handle_t dev_handle, uint8_t *abyData, size_t NDataLength);
esp_err_t eternal_clock_read_time(void);
esp_err_t eternal_clock_write_time(int year, int month, int day, int hour, int min, int sec);
esp_err_t imu_read(void);
esp_err_t imu_init(void);

#define SFRTIMER
#endif