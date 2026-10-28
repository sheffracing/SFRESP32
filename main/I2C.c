/*
I2C.c
File contains the functions for comunicating with a pepetual timer for a esp32c6
over I2C. Timer is a TBC (MCP7940M?)

Written by Cole Perera for Sheffield Formula Racing 2025
This software was written by humans and is licensed under GNU General Public License v3.0. See LICENSE.txt for more details.
*/

#include "I2C.h"

# ifdef I2C0_SDA
/* --------------------------- Global Variables ----------------------------- */
i2c_master_bus_handle_t stI2C0Handle;
i2c_master_dev_handle_t stI2C0Dev0Handle;

/* --------------------------- Function prototypes -------------------------- */
esp_err_t I2C_init(void);
esp_err_t I2C_write( uint8_t *abyData, size_t NDataLength);
esp_err_t I2C_read( uint8_t *abyData, size_t NDataLength);
esp_err_t eternal_clock_read_time(void);
esp_err_t eternal_clock_write_time(int year, int month, int day, int hour, int min, int sec);

/* --------------------------- Definitions ---------------------------------- */
#define I2C0_PORT -1 // Auto select I2C port
#define I2C0_IGNORE_COUNT 7 // Datasheet recomended
#define I2C0_SCL_SPEED_HZ 100000 // 100kHz standard mode
#define I2C_WRITE_TIMEOUT_MS 100 // 100 ms timeout for write operations
#define I2C_READ_TIMEOUT_MS 100 // 100 ms timeout for read operations

/* --------------------------- Local Variables ------------------------------ */
i2c_master_bus_config_t stI2C0MasterConfig = {
    .i2c_port = I2C0_PORT,
    .sda_io_num = I2C0_SDA,
    .scl_io_num = I2C0_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = I2C0_IGNORE_COUNT,
    .flags.enable_internal_pullup = TRUE, //External pullup required for high speed mode
};

i2c_device_config_t stI2C0Dev0Config = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x68,
    .scl_speed_hz = I2C0_SCL_SPEED_HZ,
};

/* --------------------------- Functions ------------------------------------ */

esp_err_t I2C_init(void)
{
    esp_err_t eStatus;
    eStatus = i2c_new_master_bus(&stI2C0MasterConfig, &stI2C0Handle);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to create I2C master bus: %s", esp_err_to_name(eStatus));
        return eStatus;
    }

    eStatus = i2c_master_bus_add_device(stI2C0Handle, &stI2C0Dev0Config, &stI2C0Dev0Handle);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to add I2C device to bus: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    return ESP_OK;  
}

esp_err_t I2C_write(uint8_t *abyData, size_t NDataLength)
{
    esp_err_t eStatus;
    eStatus = i2c_master_transmit(stI2C0Dev0Handle, abyData, NDataLength, I2C_WRITE_TIMEOUT_MS);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to read from I2C device: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    return ESP_OK;
}

esp_err_t I2C_read(uint8_t *abyData, size_t NDataLength)
{
    esp_err_t eStatus;
    eStatus = i2c_master_receive(stI2C0Dev0Handle, abyData, NDataLength, I2C_READ_TIMEOUT_MS);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to read from I2C device: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    return ESP_OK;
}

esp_err_t eternal_clock_read_time(void)
{
    uint8_t byStartReg = 0x00;
    uint8_t abyTimeData[7];
    char achTime[20];
    esp_err_t eStatus = ESP_OK;

    /* Set register pointer in slave */
    eStatus = I2C_write(&byStartReg, sizeof(byStartReg));
    eStatus = I2C_read(abyTimeData, sizeof(abyTimeData));
    
    /* if in 12 hour mode, convert to 24 hour mode */
    if (abyTimeData[2] & 0x40)
    {
        uint8_t byHours = abyTimeData[2] & 0xF;
        byHours += ((abyTimeData[2] >> 4) & 0x1) * 10;
        if (abyTimeData[2] & 0x20) 
        {
            /* PM */
            if (byHours != 12)
            {
                byHours += 12;
            }
        }
        else
        {
            /* AM */
            if (byHours == 12)
            {
                byHours = 0;
            }
        }
        abyTimeData[2] = 0x00;
        abyTimeData[2] |= ((byHours / 10) << 4);
        abyTimeData[2] |= (byHours % 10);
    }
    snprintf(achTime, sizeof(achTime), "%02X:%02X:%02X %02X/%02X/20%02X",
             abyTimeData[2] & 0x3F,  // Hours (mask 24h bit)
             abyTimeData[1] & 0x7F,  // Minutes  
             abyTimeData[0] & 0x7F,  // Seconds (mask ST bit)
             abyTimeData[4] & 0x3F,  // Day
             abyTimeData[5] & 0x1F,  // Month
             abyTimeData[6] & 0x3F); // Year

    ESP_LOGI("I2C", "%s", achTime);
    
    return eStatus;
}

esp_err_t eternal_clock_write_time(int year, int month, int day, int hour, int min, int sec)
{
    // Convert to BCD and write to RTC
    uint8_t abyTimeData[8];
    
    abyTimeData[0] = 0x00;  // Register address
    abyTimeData[1] = ((sec / 10) << 4) | (sec % 10);  // Seconds
    abyTimeData[2] = ((min / 10) << 4) | (min % 10);         // Minutes
    abyTimeData[3] = ((hour / 10) << 4) | (hour % 10);       // Hours (24h format)
    abyTimeData[4] = 0x00;  // WKDAY
    abyTimeData[5] = ((day / 10) << 4) | (day % 10);         // Day
    abyTimeData[6] = ((month / 10) << 4) | (month % 10);     // Month  
    abyTimeData[7] = ((year / 10) << 4) | (year % 10);       // Year
    
    return I2C_write(abyTimeData, sizeof(abyTimeData));
}

#endif