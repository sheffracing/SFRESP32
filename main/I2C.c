/*
I2C.c
File contains the functions for comunicating with I2C devices with a esp32c6 
including a PCF2129 RTC and a BNO055 IMU.

Written by Cole Perera and Daniel Hartley for Sheffield Formula Racing 2025
This software was written by humans and is licensed under GNU General Public License v3.0. See LICENSE.txt for more details.
*/

#include "I2C.h"

# ifdef I2C0_SDA
/* --------------------------- Global Variables ----------------------------- */
i2c_master_bus_handle_t stI2C0Handle;
i2c_master_dev_handle_t stI2C0Dev0Handle; //RTC PCF2129
i2c_master_dev_handle_t stI2C0Dev1Handle; //IMU BNO055

/* --------------------------- Function prototypes -------------------------- */
esp_err_t I2C_init(void);
esp_err_t I2C_write(i2c_master_dev_handle_t dev_handle, uint8_t *abyData, size_t NDataLength);
esp_err_t I2C_read(i2c_master_dev_handle_t dev_handle, uint8_t *abyData, size_t NDataLength);
esp_err_t eternal_clock_read_time(void);
esp_err_t eternal_clock_write_time(int year, int month, int day, int hour, int min, int sec);
esp_err_t imu_read(void);
esp_err_t imu_init(void);

/* --------------------------- Definitions ---------------------------------- */
#define I2C0_PORT -1 // Auto select I2C port
#define I2C0_IGNORE_COUNT 7 // Datasheet recomended
#define I2C0_SCL_SPEED_HZ 100000 // 100kHz standard mode
#define I2C_WRITE_TIMEOUT_MS 100 // 100 ms timeout for write operations
#define I2C_READ_TIMEOUT_MS 100 // 100 ms timeout for read operations

// BNO055 Registers
#define BNO055_OPR_MODE_REG 0x3D
#define BNO055_EULER_H_LSB  0x1A
#define BNO055_LIA_X_LSB    0x28

/* --------------------------- Local Variables ------------------------------ */
i2c_master_bus_config_t stI2C0MasterConfig = {
    .i2c_port = I2C0_PORT,
    .sda_io_num = I2C0_SDA,
    .scl_io_num = I2C0_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = I2C0_IGNORE_COUNT,
    .flags.enable_internal_pullup = TRUE, //External pullup required for high speed mode
};

i2c_device_config_t stI2C0Dev0Config = { //RTC PCF2129
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x51,
    .scl_speed_hz = I2C0_SCL_SPEED_HZ,
};
extern uint8_t atRealTime[7]; 
extern float imuData[6]; 

i2c_device_config_t stI2C0Dev1Config = { //IMU BNO055
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x28,
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

    eStatus = i2c_master_bus_add_device(stI2C0Handle, &stI2C0Dev1Config, &stI2C0Dev1Handle);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to add I2C device to bus: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    imu_init(); //Sets BNO055 registers for use

    /* PCF2127 control registers are cleared once on power-up so the RTC runs. */
    /*uint8_t controlRegs[4] = {0x00, 0x00, 0x00, 0x00};
    eStatus = I2C_write(controlRegs, sizeof(controlRegs));
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to initialize PCF2127 control registers: %s", esp_err_to_name(eStatus));
        return eStatus;
    }*/
    return ESP_OK;  
}

esp_err_t I2C_write(i2c_master_dev_handle_t dev_handle, uint8_t *abyData, size_t NDataLength)
{
    esp_err_t eStatus;
    eStatus = i2c_master_transmit(dev_handle, abyData, NDataLength, I2C_WRITE_TIMEOUT_MS);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to write to I2C device: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    return ESP_OK;
}

esp_err_t I2C_read(i2c_master_dev_handle_t dev_handle, uint8_t *abyData, size_t NDataLength)
{
    esp_err_t eStatus;
    eStatus = i2c_master_receive(dev_handle, abyData, NDataLength, I2C_READ_TIMEOUT_MS);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to read from I2C device: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    return ESP_OK;
}

esp_err_t eternal_clock_read_time(void)
{
    uint8_t byStartReg = 0x03;
    char achTime[20];
    esp_err_t eStatus = ESP_OK;

    // PCF2129 registers store if timer may be paused by lost power. Reset this.
    I2C_write(stI2C0Dev0Handle, &byStartReg, 1);

    /* Read the RTC registers in a single repeated-start transaction. */
    I2C_read(stI2C0Dev0Handle, atRealTime, 7); 
    /*
    eStatus = i2c_master_transmit_receive(stI2C0Dev0Handle,
                                          &byStartReg,
                                          sizeof(byStartReg),
                                          atRealTime,
                                          sizeof(atRealTime),
                                          I2C_READ_TIMEOUT_MS);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("I2C", "Failed to read from I2C device: %s", esp_err_to_name(eStatus));
        return eStatus;
    }*/

    /* if in 12 hour mode, convert to 24 hour mode */
    if (atRealTime[2] & 0x40)
    {
        uint8_t byHours = atRealTime[2] & 0xF;
        byHours += ((atRealTime[2] >> 4) & 0x1) * 10;
        if (atRealTime[2] & 0x20) 
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
        atRealTime[2] = 0x00;
        atRealTime[2] |= ((byHours / 10) << 4);
        atRealTime[2] |= (byHours % 10); 
    }
    
    /* Post-process time data */
    atRealTime[2] &= 0x3F;  // Hours (mask 24h bit)
    atRealTime[1] &= 0x7F;  // Minutes  
    atRealTime[0] &= 0x7F;  // Seconds (mask ST bit)
    atRealTime[3] &= 0x3F;  // Day
    atRealTime[5] &= 0x1F;  // Month
    atRealTime[6] &= 0x3F;  // Year
    snprintf(achTime, sizeof(achTime), "%02X:%02X:%02X %02X/%02X/20%02X",
            atRealTime[2] & 0x3F,  // Hours (mask 24h bit)           
            atRealTime[1] & 0x7F,  // Minutes  
            atRealTime[0] & 0x7F,  // Seconds (mask ST bit)
            atRealTime[3] & 0x3F,  // Day
            atRealTime[5] & 0x1F,  // Month
            atRealTime[6] & 0xFF); // Year

    ESP_LOGI("I2C", "%s", achTime);
    
    return eStatus;
}

esp_err_t eternal_clock_write_time(int year, int month, int day, int hour, int min, int sec)
{
    // Convert to BCD and write to RTC
    uint8_t atFalshedTime[8];
    
    atFalshedTime[0] = 0x03;  // Register address
    atFalshedTime[1] = 0x7F & (((sec / 10) << 4) | (sec % 10));  // Seconds
    atFalshedTime[2] = ((min / 10) << 4) | (min % 10);         // Minutes
    atFalshedTime[3] = ((hour / 10) << 4) | (hour % 10);       // Hours (24h format)
    atFalshedTime[4] = ((day / 10) << 4) | (day % 10);         // Day
    atFalshedTime[5] = 0x00;  // WKDAY
    atFalshedTime[6] = ((month / 10) << 4) | (month % 10);     // Month  
    atFalshedTime[7] = ((year / 10) << 4) | (year % 10);       // Year
    
    return I2C_write(stI2C0Dev0Handle, atFalshedTime, sizeof(atFalshedTime));
}

esp_err_t imu_init(void)
{
    uint8_t init_data[2] = { BNO055_OPR_MODE_REG, 0x0C }; // 0x0C sets NDOF mode
    
    I2C_write(stI2C0Dev1Handle, init_data, sizeof(init_data));

    ESP_LOGI("I2C", "BNO055 calibrated and active.");
    return ESP_OK;
}

esp_err_t imu_read(void)
{
    uint8_t bnoOriBuf[6];
    uint8_t bnoAccBuf[6];
    esp_err_t eStatus;

    // --- READ BNO055 ORIENTATION ---
    // Pass the target register address inline
    uint8_t ori_reg = BNO055_EULER_H_LSB;
    eStatus = i2c_master_transmit_receive(stI2C0Dev1Handle, &ori_reg, 1, bnoOriBuf, 6, I2C_READ_TIMEOUT_MS);
    if (eStatus != ESP_OK) 
    {
        ESP_LOGE("I2C", "Error reading Euler Orientation data");
        return eStatus;
    }

    int16_t headingRaw = (int16_t)(bnoOriBuf[0] | (bnoOriBuf[1] << 8));
    int16_t rollRaw    = (int16_t)(bnoOriBuf[2] | (bnoOriBuf[3] << 8));
    int16_t pitchRaw   = (int16_t)(bnoOriBuf[4] | (bnoOriBuf[5] << 8));

    imuData[0] = (float)headingRaw / 16.0f; //heading
    imuData[1] = (float)rollRaw / 16.0f; //roll
    imuData[2] = (float)pitchRaw / 16.0f; //pitch

    // --- READ BNO055 LINEAR ACCELERATION ---
    // Pass the target register address inline
    uint8_t acc_reg = BNO055_LIA_X_LSB;
    eStatus = i2c_master_transmit_receive(stI2C0Dev1Handle, &acc_reg, 1, bnoAccBuf, 6, I2C_READ_TIMEOUT_MS);
    if (eStatus != ESP_OK) 
    {
        ESP_LOGE("I2C", "Error reading Linear Acceleration data");
        return eStatus;
    }

    int16_t accXRaw = (int16_t)(bnoAccBuf[0] | (bnoAccBuf[1] << 8));
    int16_t accYRaw = (int16_t)(bnoAccBuf[2] | (bnoAccBuf[3] << 8));
    int16_t accZRaw = (int16_t)(bnoAccBuf[4] | (bnoAccBuf[5] << 8));

    imuData[3] = (float)accXRaw / 100.0f; //accX
    imuData[4] = (float)accYRaw / 100.0f; //accY
    imuData[5] = (float)accZRaw / 100.0f; //accZ

    // Log the read values 
    //ESP_LOGI("I2C", "Heading: %.2f | Roll: %.2f | Pitch: %.2f", imuData[0], imuData[1], imuData[2]);
    //ESP_LOGI("I2C", "Acc X: %.2f | Acc Y: %.2f | Acc Z: %.2f", imuData[3], imuData[4], imuData[5]);

    return ESP_OK;
}