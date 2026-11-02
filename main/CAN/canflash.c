/*
canflash.c
File contains functions to update firmware over CAN bus for esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2026
This software was written by humans and is licensed under GNU General Public License v3.0. See LICENSE.txt for more details.
*/
#include "canflash.h"

/* --------------------------- Global Variables ----------------------------- */
uint32_t NBytesWrittenReflash = 0;
uint16_t NErrorCountReflash = 0;
uint32_t NFirmwareSize = 0;

/* --------------------------- Definitions ---------------------------------- */
#define CRC8_POLYNOMIAL 0x12F  //CRC-8-AUTOSTAR polynomial
#define REFLASH_QUEUE_LENGTH 23

/* --------------------------- Local Types ---------------------------------- */


/* --------------------------- Local Variables ------------------------------ */
QueueHandle_t xReflashRingBuffer = NULL;

/* Local Function Prototypes */
void reflash_enqueue_frame(const CAN_frame_t *stFrame, uint16_t NBytes);

/* --------------------------- Functions ------------------------------------ */
esp_err_t CAN_flash_init()
{
    /*
    *===========================================================================
    *   CAN_flash_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Initializes variables for CAN flash process.
    *=========================================================================== 
    *   Revision History:
    *   09/01/26 CP Initial Version
    *   
    *===========================================================================
    */

    /* Allocate Ring Buffer */
    xReflashRingBuffer = xQueueCreate(REFLASH_QUEUE_LENGTH, sizeof(uint8_t));
    if (xReflashRingBuffer == NULL) {
        ESP_LOGE("CAN", "Failed to create CAN Queue");
        return ESP_ERR_NO_MEM;
    } 
    return ESP_OK;
}

esp_err_t CAN_flash_empty_queue(esp_partition_t *stOTAPartition)
{
    /*
    *===========================================================================
    *   CAN_flash_empty_queue
    *   Takes:   Target partition to write to.
    * 
    *   Returns: None
    * 
    *   Empties the can rx buffer and writes the data into the ota partition.
    *   Flash likes to be written in 16 byte chunks.
    *=========================================================================== 
    *   Revision History:
    *   03/01/26 CP Initial Version
    *   09/01/26 CP Added CRC check and ACK/NACK response
    *===========================================================================
    */
    CAN_frame_t stCANFrame;
    CAN_frame_t stCANTxFrame;
    stCANTxFrame.dwID = DEVICE_ID;
    esp_err_t eState = ESP_OK;
    uint64_t NCANData = 0;

    if (!xCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    } 

    /* Read CAN messages and fill reflash buffer */
    while(xQueueReceive(xCANRingBuffer, &stCANFrame, 0) == pdTRUE)
    {
        if(stCANFrame.dwID != DEVICE_ID)
        {
            continue;
        }
        memcpy(&NCANData, stCANFrame.abData, stCANFrame.byDLC);
        if (crc8(NCANData) == 0)
        {
            /* Valid CRC */
            reflash_enqueue_frame(&stCANFrame, stCANFrame.byDLC - 1);

            /* Send ACK */
            stCANTxFrame.byDLC = 1;
            stCANTxFrame.abData[0] = 0xFF;
            eState = CAN_transmit(stCANBus0, &stCANTxFrame);
        } else
        {
            /* Invalid CRC - Send NACK and error count */
            stCANTxFrame.byDLC = 3;
            stCANTxFrame.abData[0] = 0x00;
            stCANTxFrame.abData[1] = (uint8_t)(NErrorCountReflash & 0xFF);
            stCANTxFrame.abData[2] = (uint8_t)((NErrorCountReflash >> 8) & 0xFF);
            eState = CAN_transmit(stCANBus0, &stCANTxFrame);
            NErrorCountReflash++;
        }
    }
    return eState;
}

esp_err_t CAN_flash_write(esp_partition_t *stOTAPartition)
{
    /*
    *===========================================================================
    *   CAN_flash_write
    *   Takes:   Target partition to write to.
    * 
    *   Returns: None
    * 
    *   Empties the can rx buffer and writes the data into the ota partition.
    *   Flash likes to be written in 16 byte chunks.
    *=========================================================================== 
    *   Revision History:
    *   03/01/26 CP Initial Version
    *   09/01/26 CP Added CRC check and ACK/NACK response
    *===========================================================================
    */
   static uint64_t tFlashingProgess = 0;
   uint8_t aFlashBuffer[16];
   esp_err_t eState = ESP_OK;

    if (stOTAPartition == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (xReflashRingBuffer == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    /* Write reflash buffer to flash in 16 byte chunks */
    if (uxQueueMessagesWaiting(xReflashRingBuffer) >= 16)
    {
        /* Get 16 bytes from reflash buffer */
        for (uint16_t NCounter = 0; NCounter < 16; NCounter++)
        {
            xQueueReceive(xReflashRingBuffer, &aFlashBuffer[NCounter], 0);
        }

        /* Write to flash */
        eState = esp_partition_write(stOTAPartition, NBytesWrittenReflash, aFlashBuffer, 16);
        
        if (eState != ESP_OK)
        {
            ESP_LOGE("CANFLASH", "Failed to write to OTA partition: %s", esp_err_to_name(eState));
            return eState;
        }
        NBytesWrittenReflash += 16;
    }

    /* Detect if there are less then 16 bytes till the end of the binary. (would get stuck otherwise) */
    if (NBytesWrittenReflash + 16 > NFirmwareSize && NFirmwareSize > 0)
    {
        uint16_t NCounter = 0;
        while (xQueueReceive(xReflashRingBuffer, &aFlashBuffer[NCounter], 0) == pdTRUE)
        {
            NCounter++;
        }
        /* Write to flash */
        eState = esp_partition_write(stOTAPartition, NBytesWrittenReflash, aFlashBuffer, NCounter+1);
        
        if (eState != ESP_OK)
        {
            ESP_LOGE("CANFLASH", "Failed to write to OTA partition: %s", esp_err_to_name(eState));
            return eState;
        }
        NBytesWrittenReflash += NCounter+1;
    }

    /* Debug Reporting */
    if (esp_timer_get_time() - tFlashingProgess >= 500000) //Every 0.5s
    {
        tFlashingProgess = esp_timer_get_time();
        ESP_LOGI("CANFLASH", "Reflash Progress: %d / %d bytes written, %d waiting in queue",
            (uint32_t)NBytesWrittenReflash, (uint32_t)NFirmwareSize, uxQueueMessagesWaiting(xReflashRingBuffer));
    }
    
    return ESP_OK;
}

uint16_t crc8(uint64_t NData)
{
    /*
    *===========================================================================
    *   crc8
    *   Takes:   64 bit data word.
    * 
    *   Returns: 8 bit CRC value.
    * 
    *   Calculates an 8 bit CRC for the given 64 bit data word.
    *=========================================================================== 
    *   Revision History:
    *   08/01/26 CP Initial Version
    *   
    *===========================================================================
    */
    
    uint16_t NOffset = 63;
    while (NOffset >= 8)
    {
        if (NData & ((uint64_t)1 << NOffset))
        {
            NData = ((uint64_t)CRC8_POLYNOMIAL << (NOffset - 8)) ^ NData;
        }
        NOffset--;
    }

    return (uint16_t)(NData & 0xFF);
}

void reflash_enqueue_frame(const CAN_frame_t *stFrame, uint16_t NBytes)
{
    /*
    *===========================================================================
    *   reflash_enqueue_frame
    *   Takes:   Pointer to CAN frame to enqueue.
    *            Number of bytes to include.   
    * 
    *   Returns: Nothing.
    * 
    *   Enqueues the data bytes of the given CAN frame into the reflash queue.
    *=========================================================================== 
    *   Revision History:
    *   09/01/26 CP Initial Version
    *   
    *===========================================================================
    */
    if (!xReflashRingBuffer) 
    {
        return;
    }
    if (NBytes > stFrame->byDLC)
    {
        NBytes = stFrame->byDLC;
    }

    for (uint8_t NCounter = 0; NCounter < NBytes; NCounter++)
    {
        xQueueSend(xReflashRingBuffer, &stFrame->abData[NCounter], 0);
    }
}