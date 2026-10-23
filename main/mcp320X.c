/*
mcp320X.c
File contains the MCP3204/8 interface over spi.

Written by Cole Perera for Sheffield Formula Racing 2026
This software was written by humans and is licensed under GNU General Public License v3.0. See LICENSE.txt for more details.
*/

#include "mcp320X.h"

/* --------------------------- Local Types ---------------------------- */
spi_device_handle_t stMCP3204_1;
spi_device_handle_t stMCP3204_2;
spi_device_handle_t stMCP3208;

/* --------------------------- Local Variables ------------------------ */


/* --------------------------- Global Variables ----------------------- */


/* --------------------------- Definitions ---------------------------- */
#define MCP3208_SPI_FREQ 1000000 // 1 MHz
#define VADC_REFERNCE 5.0



/* --------------------------- Functions ------------------------------ */
esp_err_t MCP320X_init(uint8_t NCSPin, spi_device_handle_t stDeviceHandle)
{
    /*
    *===========================================================================
    *   MCP320X_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Initializes the MCP3204/8 devices over spi.
    *===========================================================================
    *   Revision History:
    *   24/02/26 CP Initial Version
    *   02/03/26 CP Supports multiple devices
    *   23/03/26 CP Switched back to single device. 
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    word wNCounter = 0;
    spi_device_interface_config_t stDeviceConfig = 
    {
        .clock_speed_hz = MCP3208_SPI_FREQ,
        .mode = 0,
        .command_bits = 0,
        .address_bits = 0, 
        .queue_size = 5,
        .spics_io_num = NCSPin,
    };

    eStatus = spi_bus_add_device(SPI2_HOST, &stDeviceConfig, &stDeviceHandle);
    if (eStatus != ESP_OK)
    {
        ESP_LOGE(SFR_TAG, "Failed to add MCP320X device: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    return eStatus;
}

float MCP320X_read(spi_device_handle_t stDeviceHandle, uint8_t NDevADC)
{
    /*
    *===========================================================================
    *   MCP320X_read
    *   Takes:   stDeviceHandle: Handle to the SPI device
    *            NDevADC: ADC identifier on the device (0-3 for MCP3204, 0-7 for MCP3208)
    * 
    *   Returns: ADC voltage after convertsion from counts.
    * 
    *   Sends a command to the MCP3208 and reads back the ADC data.
    *===========================================================================
    *   Revision History:
    *   02/03/26 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    spi_transaction_t stSPICall;
    word wNData;
    uint8_t NDataTx[3];
    uint8_t NDataRx[3];
    float fVADCRaw; 
    if (NDevADC > 7)
    {
        return 999.9;
    }

    // Alignment Logic for 24 clocks (3 bytes):
    // Byte 0: 0 0 0 0 0 S M D2
    // Byte 1: D1 D0 N N B11 B10 B9 B8
    // Byte 2: B7 B6 B5 B4 B3 B2 B1 B0
    // Where S is start bit (1), M is mode (0 for single-ended, 1 for differential), D2-D0 are the channel selection bits, 
    // N is null bit, and B11-B0 are the 12-bit ADC result.

    memset(&stSPICall, 0, sizeof(stSPICall));
    NDataTx[0] = 0x6 | ((NDevADC & 0x4) >> 2);
    NDataTx[1] = (NDevADC & 0x3) << 6;
    NDataTx[2] = 0x00;
    stSPICall.length = 24;
    stSPICall.tx_buffer = NDataTx;
    stSPICall.rx_buffer = NDataRx;
    eStatus = spi_device_transmit(stDeviceHandle, &stSPICall);
    if (eStatus != ESP_OK)    {
        return 999.9;
    }
    wNData = ((NDataRx[1] & 0xF) << 8) | NDataRx[2];
    fVADCRaw = (float)wNData / 4096.0 * (float)VADC_REFERNCE;
    return fVADCRaw;

}