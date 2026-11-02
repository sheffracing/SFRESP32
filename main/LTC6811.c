/*
LTC6811HG-1.c
File contains the LTC6811HG-1 interface over iso-spi via a LTC6820HMS.

Written by Cole Perera for Sheffield Formula Racing 2026
*/

#include "LTC6811.h"

/* --------------------------- Local Types ---------------------------- */


/* --------------------------- Local Variables ------------------------ */


/* --------------------------- Definitions ---------------------------- */
#define LTC6811_SPI_FREQ 1000000 // 1 MHz
#define CELL_VOLTAGE_MIN 2.5f // Minimum cell voltage in volts
#define CELL_VOLTAGE_MAX 4.3f // Maximum cell voltage in volts
#define BYTES_PER_DEVICE 8 // Number of bytes per device 2 PEC + 6 Data
#define BYTES_PER_REGISTER 6 // Number of bytes per register group

/* --------------------------- Global Variables ----------------------- */
uint16_t aVCell[NUM_DEVICES * CELLS_PER_DEVICE];
uint16_t tSinceVCellms[NUM_DEVICES * CELLS_PER_DEVICE]; //ms
float aVCellActual[NUM_DEVICES * CELLS_PER_DEVICE];


/* --------------------------- Functions ------------------------------ */
esp_err_t LTC6811_init(uint8_t NCSPin, spi_device_handle_t stDeviceHandle)
{
    /*
    *===========================================================================
    *   LTC6811_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Initializes the LTC6811 devices over spi.
    *===========================================================================
    *   Revision History:
    *   23/10/26 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    uint8_t aNData[BYTES_PER_REGISTER * NUM_DEVICES]; 
    uint16_t NVUV;
    uint16_t NVOV;
    uint16_t NCounter;
    spi_device_interface_config_t stDeviceConfig = 
    {
        .clock_speed_hz = LTC6811_SPI_FREQ,
        .mode = 0,
        .command_bits = 0,
        .address_bits = 0, 
        .queue_size = 5,
        .spics_io_num = NCSPin,
    };

    /* Initialise SPI Device */
    eStatus = spi_bus_add_device(SPI2_HOST, &stDeviceConfig, &stDeviceHandle);
    if (eStatus != ESP_OK)
        {
            ESP_LOGE(SFR_TAG, "Failed to add LTC6811 device: %s", esp_err_to_name(eStatus));
            return eStatus;
        }

    /* Initialise LTC6811 Devices */
    aNData[5] = CFGR0;
    aNData[4] = VUV & 0xFF;
    aNData[3] = ((VUV >> 8) & 0xF) | ((VOV & 0xF) << 4);
    aNData[2] = (VOV >> 4) & 0xFF;
    aNData[1] = 0x00;
    aNData[0] = 0x00; 

    for( NCounter = 1; NCounter < NUM_DEVICES; NCounter++)
    {
        memcpy(&aNData[NCounter * BYTES_PER_REGISTER], &aNData[0], BYTES_PER_REGISTER);
    }

    BC_Write(stDeviceHandle, WRCFGA, (BYTES_PER_REGISTER * NUM_DEVICES), &aNData);
    return eStatus;
}

esp_err_t BC_Write(spi_device_handle_t stDeviceHandle, uint16_t CMD, uint16_t NBytes, uint8_t *aData)
{
    /*
    *===========================================================================
    *   BC_Write
    *   Takes:   CMD - Command to send to LTC6811
    *            NBytes - Number of bytes to send
    *            aData - Pointer to data to send
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Writes a command and data to the LTC6811 over iso-spi.
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
    uint8_t NDataTx[4 + NBytes + 2 * (NBytes / BYTES_PER_REGISTER)]; // 2 CMD, 2 PEC, NBytes data, 2 PEC for every 6 bytes of data
    
    for (NCounter = 0; NCounter < NBytes; NCounter += BYTES_PER_REGISTER)
    {
        /* 6 Data Bytes */
        NOffset = NCounter + 2 * (NCounter / BYTES_PER_REGISTER);
        memcpy(&NDataTx[NOffset], &aData[NCounter], BYTES_PER_REGISTER);
        
        /* 2 Byte PEC for every 6 bytes of data */
        NDataTx[NOffset + BYTES_PER_REGISTER + 0] = (PEC15_calc(BYTES_PER_REGISTER, &aData[NCounter]) >> 8) & 0xFF;
        NDataTx[NOffset + BYTES_PER_REGISTER + 1] = PEC15_calc(BYTES_PER_REGISTER, &aData[NCounter]) & 0xFF;
    }

    /* 2 Byte PEC for the command */
    NDataTx[NBytes + 0] = (PEC15_calc(2, CMD) >> 8) & 0xFF;
    NDataTx[NBytes + 1] = PEC15_calc(2, CMD) & 0xFF;

    /* 2 Byte Command */
    NDataTx[NBytes + 2] = (CMD >> 8) & 0xFF;
    NDataTx[NBytes + 3] = CMD & 0xFF;

    /* Transmit */
    memset(&stSPICall, 0, sizeof(stSPICall));
    stSPICall.length = 8 * (4 + NBytes + 2 * (NBytes / BYTES_PER_REGISTER));
    stSPICall.tx_buffer = NDataTx;
    eStatus = spi_device_transmit(stDeviceHandle, &stSPICall);
    return eStatus;
}

esp_err_t BC_Read(spi_device_handle_t stDeviceHandle, uint16_t CMD, uint16_t NBytes, uint8_t *aData)
{
    /*
    *===========================================================================
    *   BC_Read
    *   Takes:   CMD - Command to send to LTC6811
    *            NBytes - Number of bytes to receive
    *            aData - Pointer to empty buffer to store received data
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Reads a command and data from the LTC6811 over iso-spi.
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
    uint8_t NDataTx[4]; // 2 CMD, 2 PEC
    uint8_t NDataRx[NBytes + 2 * (NBytes / BYTES_PER_REGISTER)]; // 2 bytes PEC for every 8 bytes of data
    uint16_t NRxPEC;

    /* 2 Byte PEC */
    NDataTx[0] = (PEC15_calc(2, &NDataTx) >> 8) & 0xFF; // Calculate PEC for command
    NDataTx[1] = PEC15_calc(2, &NDataTx) & 0xFF;
    
    /* 2 Byte Command */
    NDataTx[2] = (CMD >> 8) & 0xFF;
    NDataTx[3] = CMD & 0xFF;

    memset(&stSPICall, 0, sizeof(stSPICall));
    stSPICall.length = 8 * (4 + NBytes + 2 * (NBytes / BYTES_PER_REGISTER));
    stSPICall.tx_buffer = NDataTx;
    stSPICall.rx_buffer = NDataRx;
    eStatus = spi_device_transmit(stDeviceHandle, &stSPICall);
    if (eStatus != ESP_OK) { return eStatus; }

    /* Check the received data */
    for (NCounter = 0; NCounter < NBytes; NCounter += BYTES_PER_DEVICE)
    {
        /* 8 Data Bytes */
        NOffset = NCounter + 2 * (NCounter / BYTES_PER_DEVICE);
        /* Check PEC for every 8 bytes of data */
        NRxPEC = (NDataRx[NOffset + BYTES_PER_DEVICE] << 8) | NDataRx[NOffset + BYTES_PER_DEVICE + 1];
        if (PEC15_calc(8, &NDataRx[NOffset]) != NRxPEC)
        {
            return ESP_ERR_INVALID_CRC;
        }
    }

    /* Data ok copy to provided buffer */
    for (NCounter = 0; NCounter < NBytes; NCounter += BYTES_PER_DEVICE)
    {
        NOffset = NCounter + 2 * (NCounter / BYTES_PER_DEVICE);
        memcpy(&aData[NCounter], &NDataRx[NOffset], BYTES_PER_DEVICE);
    }

    return eStatus;
}

esp_err_t read_VCell(spi_device_handle_t stDeviceHandle)
{
    /*
    *===========================================================================
    *   read_VCell
    *   Takes:   stDeviceHandle - SPI device handle for LTC6811
    *            
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Reads the cell voltages from the LTC6811 devices. Stores voltages in the global array aVCell in millivolts.
    *===========================================================================
    *   Revision History:
    *   27/10/26 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;


    return eStatus;
}