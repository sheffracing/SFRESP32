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
#define CELLS_PER_GROUP 3 // Number of cells per register group
#define VUV 0x0000 // Under voltage threshold
#define VOV 0x0000 // Over voltage threshold

/* --------------------------- Global Variables ----------------------- */
spi_device_handle_t stLTC6811Handle;


/* --------------------------- Functions ------------------------------ */
esp_err_t LTC6811_init(uint8_t NCSPin)
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
    uint16_t NVUV = 0;
    uint16_t NVOV = 0;
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
    eStatus = spi_bus_add_device(SPI2_HOST, &stDeviceConfig, &stLTC6811Handle);
    if (eStatus != ESP_OK)
        {
            ESP_LOGE(SFR_TAG, "Failed to add LTC6811 device: %s", esp_err_to_name(eStatus));
            return eStatus;
        }

    /* Initialise LTC6811 Devices */
    aNData[5] = CFGR0;
    aNData[4] = NVUV & 0xFF;
    aNData[3] = ((NVUV >> 8) & 0xF) | ((NVOV & 0xF) << 4);
    aNData[2] = (NVOV >> 4) & 0xFF;
    aNData[1] = 0x00;
    aNData[0] = 0x00; 

    for( NCounter = 1; NCounter < NUM_DEVICES; NCounter++)
    {
        memcpy(&aNData[NCounter * BYTES_PER_REGISTER], &aNData[0], BYTES_PER_REGISTER);
    }

    BC_Write(WRCFGA, (BYTES_PER_REGISTER * NUM_DEVICES), aNData);
    return eStatus;
}

esp_err_t BC_Write(uint16_t CMD, uint16_t NBytes, uint8_t *aData)
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
    uint32_t NPEC;
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

    /* 2 Byte Command */
    NDataTx[NBytes + 2] = (CMD >> 8) & 0xFF;
    NDataTx[NBytes + 3] = CMD & 0xFF;

    /* 2 Byte PEC for the command */
    NPEC = PEC15_calc(2, &NDataTx[NBytes + 2]);
    NDataTx[NBytes + 0] = (NPEC >> 8) & 0xFF;
    NDataTx[NBytes + 1] = NPEC & 0xFF;

    /* Transmit */
    memset(&stSPICall, 0, sizeof(stSPICall));
    stSPICall.length = 8 * (4 + NBytes + 2 * (NBytes / BYTES_PER_REGISTER));
    stSPICall.tx_buffer = NDataTx;
    eStatus = spi_device_transmit(stLTC6811Handle, &stSPICall);
    return eStatus;
}

esp_err_t BC_Read(uint16_t CMD, uint8_t *aData)
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
    uint16_t NCounter;
    uint16_t NOffset = 0;
    uint8_t NDataTx[4]; // 2 CMD, 2 PEC
    uint16_t NBytes = BYTES_PER_REGISTER * NUM_DEVICES;
    uint8_t NDataRx[NBytes + 2 * (NBytes / BYTES_PER_REGISTER)]; // 2 bytes PEC for every 8 bytes of data
    uint16_t NRxPEC;
    uint16_t NTxPEC;
    
    /* 2 Byte Command */
    NDataTx[2] = (CMD >> 8) & 0xFF;
    NDataTx[3] = CMD & 0xFF;

    /* 2 Byte PEC for the command */
    NTxPEC = PEC15_calc(2, &NDataTx[2]);
    NDataTx[0] = (NTxPEC >> 8) & 0xFF;
    NDataTx[1] = NTxPEC & 0xFF;

    /* SPI Transaction */
    memset(&stSPICall, 0, sizeof(stSPICall));
    stSPICall.length = 8 * (4 + NBytes + 2 * (NBytes / BYTES_PER_REGISTER));
    stSPICall.tx_buffer = NDataTx;
    stSPICall.rx_buffer = NDataRx;
    eStatus = spi_device_transmit(stLTC6811Handle, &stSPICall);
    if (eStatus != ESP_OK) { return eStatus; }

    /* Check the received data */
    for (NCounter = 0; NCounter < NBytes; NCounter += BYTES_PER_DEVICE)
    {
        /* 8 Data Bytes */
        NOffset = NCounter + 2 * (NCounter / BYTES_PER_DEVICE);
        /* Check PEC for every 8 bytes of data */
        NRxPEC = (NDataRx[NOffset + BYTES_PER_DEVICE] << 8) | NDataRx[NOffset + BYTES_PER_DEVICE + 1];
        // if (PEC15_calc(8, &NDataRx[NOffset]) != NRxPEC)
        // {
        //     return ESP_ERR_INVALID_CRC;
        // }
    }

    /* Data ok copy to provided buffer */
    for (NCounter = 0; NCounter < NBytes; NCounter += BYTES_PER_DEVICE)
    {
        NOffset = NCounter + 2 * (NCounter / BYTES_PER_DEVICE);
        memcpy(&aData[NCounter], &NDataRx[NOffset], BYTES_PER_DEVICE);
    }

    return eStatus;
}

esp_err_t read_VCell(uint16_t NCMD, uint8_t *aNData)
{
    /*
    *===========================================================================
    *   read_VCell
    *   Takes:   NCMD - Command to send to the LTC6811
    *            aNData - Pointer to the data in the read buffer
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Reads the cell voltages from the LTC6811 devices. Stores voltages in the global array aVCell.
    *===========================================================================
    *   Revision History:
    *   27/10/26 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    uint8_t NGroup;
    uint16_t aVCells[3];

    /* Extract the cell group */
    switch (NCMD)
    {
        case RDCVA:
            NGroup = 0;
            break;
        case RDCVB:
            NGroup = 1;
            break;
        case RDCVC:
            NGroup = 2;
            break;
        case RDCVD:
            NGroup = 3;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    for (uint16_t NCounter = 0; NCounter < NUM_DEVICES; NCounter++)
    {
        aVCells[0] = (aNData[BYTES_PER_REGISTER * NCounter + 0] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 1] << 8);
        aVCells[1] = (aNData[BYTES_PER_REGISTER * NCounter + 2] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 3] << 8);
        aVCells[2] = (aNData[BYTES_PER_REGISTER * NCounter + 4] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 5] << 8);
        
        VCellRaw[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 0] = aVCells[0];
        VCellRaw[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 1] = aVCells[1];
        VCellRaw[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 2] = aVCells[2];

        tSinceVCellms[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 0] = 0;
        tSinceVCellms[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 1] = 0;
        tSinceVCellms[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 2] = 0;
    }

    return eStatus;
}

esp_err_t read_Status(uint16_t NCMD, uint8_t *aNData)
{
    /*
    *===========================================================================
    *   read_VCell
    *   Takes:   NCMD - Command to send to the LTC6811
    *            aNData - Pointer to the data in the read buffer
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Reads the cell voltages from the LTC6811 devices. Stores voltages in the global array aVCell.
    *===========================================================================
    *   Revision History:
    *   27/10/26 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    uint8_t NGroup;
    uint16_t aVCells[3];

    /* Extract the cell group */
    switch (NCMD)
    {
        case RDCVA:
            NGroup = 0;
            break;
        case RDCVB:
            NGroup = 1;
            break;
        case RDCVC:
            NGroup = 2;
            break;
        case RDCVD:
            NGroup = 3;
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    for (uint16_t NCounter = 0; NCounter < NUM_DEVICES; NCounter++)
    {
        aVCells[0] = (aNData[BYTES_PER_REGISTER * NCounter + 0] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 1] << 8);
        aVCells[1] = (aNData[BYTES_PER_REGISTER * NCounter + 2] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 3] << 8);
        aVCells[2] = (aNData[BYTES_PER_REGISTER * NCounter + 4] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 5] << 8);
        
        VCellRaw[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 0] = aVCells[0];
        VCellRaw[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 1] = aVCells[1];
        VCellRaw[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 2] = aVCells[2];

        tSinceVCellms[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 0] = 0;
        tSinceVCellms[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 1] = 0;
        tSinceVCellms[NCounter * CELLS_PER_DEVICE + NGroup * CELLS_PER_GROUP + 2] = 0;
    }

    return eStatus;
}

esp_err_t General_Read(uint16_t NCMD)
{
    esp_err_t eStatus = ESP_OK;
    uint8_t aNData[BYTES_PER_REGISTER * NUM_DEVICES];

    /* Read Data */
    eStatus = BC_Read(NCMD, aNData);
    if (eStatus != ESP_OK) { return eStatus; }

    /* Pass to revelvent decode function */
    switch (NCMD)
    {
        case RDCVA:
            read_VCell(NCMD, aNData);
            break;
        case RDCVB:
            read_VCell(NCMD, aNData);
            break;
        case RDCVC:
            read_VCell(NCMD, aNData);
            break;
        case RDCVD:
            read_VCell(NCMD, aNData);
            break;
        case RDAXA:
            read_Aux(NCMD, aNData);
            break;
        case RDAXB:
            read_Aux(NCMD, aNData);
            break;
        default:
            break;
    }

    for (uint16_t NCounter = 0; NCounter < NUM_DEVICES; NCounter++)
    {  
        switch (NCMD)
        {
        case RDSTATA:   
            VDevCellSum[NCounter]       = (aNData[BYTES_PER_REGISTER * NCounter + 0] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 1] << 8);
            TDevDieInternal[NCounter]   = (aNData[BYTES_PER_REGISTER * NCounter + 2] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 3] << 8);
            VDevSupplyAnalog[NCounter]  = (aNData[BYTES_PER_REGISTER * NCounter + 4] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 5] << 8);
        break;

        case RDSTATB:
            VDevSupplyDigital[NCounter] = (aNData[BYTES_PER_REGISTER * NCounter + 0] >> 0) | (aNData[BYTES_PER_REGISTER * NCounter + 1] << 8);

            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 0] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 0 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 0] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 1 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 1] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 2 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 1] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 3 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 2] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 4 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 2] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 5 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 3] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 6 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 3] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 7 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 4] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 0 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 4] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 1 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 5] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 2 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 5] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 3 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 6] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 4 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 6] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 5 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 7] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 6 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 7] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 7 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 8] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 0 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 8] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 1 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 9] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 2 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 9] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 3 & 0x01;
            BUndervoltageCell[NCounter * CELLS_PER_DEVICE + 10] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 4 & 0x01;
            BOvervoltageCell[NCounter  * CELLS_PER_DEVICE + 10] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 5 & 0x01;
            
            BDevThemalShutdown[NCounter]  = aNData[BYTES_PER_REGISTER * NCounter + 5] >> 0 & 0x01;
            BDevMUXFail[NCounter]         = aNData[BYTES_PER_REGISTER * NCounter + 5] >> 1 & 0x01;
            NDevHWRevision[NCounter]      = aNData[BYTES_PER_REGISTER * NCounter + 5] >> 4 & 0x0F; 
            break;

        case RDCFGA:
            
            //Placeholder
            break;

        case RDSCTRL:
            NSPinState[NCounter * CELLS_PER_DEVICE + 0] = aNData[BYTES_PER_REGISTER * NCounter + 0] >> 0 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 1] = aNData[BYTES_PER_REGISTER * NCounter + 0] >> 4 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 2] = aNData[BYTES_PER_REGISTER * NCounter + 1] >> 0 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 3] = aNData[BYTES_PER_REGISTER * NCounter + 1] >> 4 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 4] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 0 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 5] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 4 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 6] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 0 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 7] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 4 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 8] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 0 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 9] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 4 & 0x0F;
            NSPinState[NCounter * CELLS_PER_DEVICE + 10] = aNData[BYTES_PER_REGISTER * NCounter + 5] >> 0 & 0x0F;
            break;

        case RDPWM:
            NPWMState[NCounter * CELLS_PER_DEVICE + 0] = aNData[BYTES_PER_REGISTER * NCounter + 0] >> 0 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 1] = aNData[BYTES_PER_REGISTER * NCounter + 0] >> 4 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 2] = aNData[BYTES_PER_REGISTER * NCounter + 1] >> 0 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 3] = aNData[BYTES_PER_REGISTER * NCounter + 1] >> 4 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 4] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 0 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 5] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 4 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 6] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 0 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 7] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 4 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 8] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 0 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 9] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 4 & 0x0F;
            NPWMState[NCounter * CELLS_PER_DEVICE + 10] = aNData[BYTES_PER_REGISTER * NCounter + 5] >> 0 & 0x0F;
            break;

        case RDCOMM:
            NDevICCO0[NCounter] = aNData[BYTES_PER_REGISTER * NCounter + 0] >> 4 & 0xF;
            NDevICCO1[NCounter] = aNData[BYTES_PER_REGISTER * NCounter + 2] >> 4 & 0xF;
            NDevICCO2[NCounter] = aNData[BYTES_PER_REGISTER * NCounter + 4] >> 4 & 0xF;

            NDevShiftByte0[NCounter] = ((aNData[BYTES_PER_REGISTER * NCounter + 0] >> 0 & 0xF) << 4) | (aNData[BYTES_PER_REGISTER * NCounter + 1] >> 4 & 0xF);
            NDevShiftByte1[NCounter] = ((aNData[BYTES_PER_REGISTER * NCounter + 2] >> 0 & 0xF) << 4) | (aNData[BYTES_PER_REGISTER * NCounter + 3] >> 4 & 0xF);
            NDevShiftByte2[NCounter] = ((aNData[BYTES_PER_REGISTER * NCounter + 4] >> 0 & 0xF) << 4) | (aNData[BYTES_PER_REGISTER * NCounter + 5] >> 4 & 0xF);

            NDevFCCO0[NCounter] = aNData[BYTES_PER_REGISTER * NCounter + 1] >> 0 & 0xF;
            NDevFCCO1[NCounter] = aNData[BYTES_PER_REGISTER * NCounter + 3] >> 0 & 0xF;
            NDevFCCO2[NCounter] = aNData[BYTES_PER_REGISTER * NCounter + 5] >> 0 & 0xF;
            break;

        default:
            return ESP_ERR_INVALID_ARG;
        }
    }    
    return ESP_ERR_INVALID_STATE;
}