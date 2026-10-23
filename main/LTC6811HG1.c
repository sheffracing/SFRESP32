/*
LTC6811HG-1.c
File contains the LTC6811HG-1 interface over iso-spi via a LTC6820HMS.

Written by Cole Perera for Sheffield Formula Racing 2026
*/

#include "LTC6811HG1.h"

/* --------------------------- Local Types ---------------------------- */


/* --------------------------- Local Variables ------------------------ */


/* --------------------------- Global Variables ----------------------- */


/* --------------------------- Definitions ---------------------------- */
#define LTC6811HG1_SPI_FREQ 1000000 // 1 MHz

/* --------------------------- Functions ------------------------------ */
esp_err_t LTC6811HG1_init(uint8_t NCSPin, spi_device_handle_t stDeviceHandle)
{
    /*
    *===========================================================================
    *   LTC6811HG1_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Initializes the LTC6811HG1 devices over spi.
    *===========================================================================
    *   Revision History:
    *   23/10/26 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    spi_device_interface_config_t stDeviceConfig = 
    {
        .clock_speed_hz = LTC6811HG1_SPI_FREQ,
        .mode = 0,
        .command_bits = 0,
        .address_bits = 0, 
        .queue_size = 5,
        .spics_io_num = NCSPin,
    };

    eStatus = spi_bus_add_device(SPI2_HOST, &stDeviceConfig, &stDeviceHandle);
    if (eStatus != ESP_OK)
        {
            ESP_LOGE(SFR_TAG, "Failed to add LTC6811HG1 device: %s", esp_err_to_name(eStatus));
            return eStatus;
        }
    return eStatus;
}

esp_err_t BC_Write(spi_device_handle_t stDeviceHandle, uint16_t CMD, uint16_t NBytes, uint8_t *aData)
{
    /*
    *===========================================================================
    *   BC_Write
    *   Takes:   CMD - Command to send to LTC6811HG1
    *            NBytes - Number of bytes to send
    *            aData - Pointer to data to send
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Writes a command and data to the LTC6811HG1 over iso-spi.
    *===========================================================================
    *   Revision History:
    *   23/10/26 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    spi_transaction_t stSPICall;
    word wNData;
    uint16_t NCounter;
    uint16_t NOffset = 0;
    uint8_t NDataTx[4 + NBytes + 2 * (NBytes / 8)]; // 2 CMD, 2 PEC then multiples of 8 bytes with 2 PEC each

    /* 2 Byte Command */
    NDataTx[0] = (CMD >> 8) & 0xFF;
    NDataTx[1] = CMD & 0xFF;
    
    /* 2 Byte PEC */
    NDataTx[2] = (PEC15_calc(2, &NDataTx) >> 8) & 0xFF; // Calculate PEC for command
    NDataTx[3] = PEC15_calc(2, &NDataTx) & 0xFF;
    
    for (NCounter = 0; NCounter < NBytes; NCounter++)
    {
        /* 8 Data Bytes */
        NOffset = 4 + NCounter + 2 * (NCounter / 8);
        NDataTx[NOffset + 0] = aData[NCounter + 0];
        NDataTx[NOffset + 1] = aData[NCounter + 1];
        NDataTx[NOffset + 2] = aData[NCounter + 2];
        NDataTx[NOffset + 3] = aData[NCounter + 3];
        NDataTx[NOffset + 4] = aData[NCounter + 4];
        NDataTx[NOffset + 5] = aData[NCounter + 5];
        NDataTx[NOffset + 6] = aData[NCounter + 6];
        NDataTx[NOffset + 7] = aData[NCounter + 7];
        
        /* 2 Byte PEC for every 8 bytes of data */
        NDataTx[NOffset + 8] = (PEC15_calc(8, &aData[NCounter - 8]) >> 8) & 0xFF;
        NDataTx[NOffset + 9] = PEC15_calc(8, &aData[NCounter - 8]) & 0xFF;
        
        NCounter += 8;
    }

    memset(&stSPICall, 0, sizeof(stSPICall));
    stSPICall.length = 8 * (4 + NBytes + 2 * (NBytes / 8));
    stSPICall.tx_buffer = NDataTx;
    eStatus = spi_device_transmit(stDeviceHandle, &stSPICall);
    return eStatus;
}

esp_err_t BC_Read(spi_device_handle_t stDeviceHandle, uint16_t CMD, uint16_t NBytes, uint8_t *aData);
