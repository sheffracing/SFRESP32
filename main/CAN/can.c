/*
can.c
File contains the CAN functions for a esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include <stdio.h>
#include <stdlib.h>
#include "can.h"

/* --------------------------- Global Variables ----------------------------- */
#ifdef GPIO_CAN0_TX
twai_node_handle_t stCANBus0;
#endif
#ifdef GPIO_CAN1_TX
twai_node_handle_t stCANBus1;
#endif

QueueHandle_t xCANRingBuffer = NULL;
extern QueueHandle_t xESPNOWRingBuffer;
dword dwNDroppedCANFrames = 0;

/* --------------------------- Definitions ---------------------------------- */
#define CAN0_BITRATE 1000000  // 1000kbps
#define CAN0_TX_QUEUE_LENGTH 10

#define CAN1_BITRATE 1000000  // 1 Mbps
#define CAN1_TX_QUEUE_LENGTH 10

#define MAX_CAN_TXS_PER_CALL 10


/* --------------------------- Local Types         -------------------------- */
typedef struct {
    twai_frame_t stFrame;
    uint32_t     dwData[2]; // Embedded 8-byte buffer (32-bit aligned)
} can_tx_buffer_t;

/* --------------------------- Local Variables ------------------------------ */
extern dword adwMaxTaskTime[eTASK_TOTAL];
static can_tx_buffer_t astCANTxPool[MAX_CAN_TXS_PER_CALL];
static uint8_t byCANTxPoolIndex = 0;
extern qword dwFirmwareSize;

/* --------------------------- Functions ------------------------------------ */
esp_err_t CAN_init(boolean bEnableRx)
{
    /*
    *===========================================================================
    *   CAN_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Creates and starts all defined CAN busses that have pins specifed in
    *   pin.h.
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *   29/10/25 CP Updated to use onchip driver, old driver depriecated
    *   18/11/25 CP Moved Enable RX to parameter
    *   24/11/25 CP Swapped ring buffer for freeRTOS queue
    *
    *===========================================================================
    */

    esp_err_t stState = ESP_OK;

    /* Bus 0 */
    #ifdef GPIO_CAN0_TX
    twai_onchip_node_config_t stCANNode0Config = 
    {
        .io_cfg.tx = GPIO_CAN0_TX,
        .io_cfg.rx = GPIO_CAN0_RX,
        .bit_timing.bitrate = CAN0_BITRATE,
        .tx_queue_depth = CAN0_TX_QUEUE_LENGTH,
        .intr_priority = 3,
    };
    stState = twai_new_node_onchip(&stCANNode0Config, &stCANBus0);
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN0 twai_new_node_onchip failed: %s", esp_err_to_name(stState));  
    }
    if (bEnableRx) 
    {
        twai_event_callbacks_t stRxCallback =
        {
            .on_rx_done = CAN_receive_callback,
        };
        stState = twai_node_register_event_callbacks(stCANBus0, &stRxCallback, NULL);
        if ( stState != ESP_OK )
        {
            ESP_LOGE("CAN", "CAN0 failed to register callback: %s", esp_err_to_name(stState));  
        }
    } else
    {
        twai_event_callbacks_t stRxCallback =
        {
            .on_rx_done = CAN_receive_callback_no_queue,
        };
        stState = twai_node_register_event_callbacks(stCANBus0, &stRxCallback, NULL);
        if ( stState != ESP_OK )
        {
            ESP_LOGE("CAN", "CAN0 failed to register callback: %s", esp_err_to_name(stState));  
        }
    }
    stState = twai_node_enable(stCANBus0); 
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN0 Bus failed to start: %s", esp_err_to_name(stState));  
    }

    #endif

    /* Bus 1 */
    #ifdef GPIO_CAN1_TX
    twai_onchip_node_config_t stCANNode1Config = 
    {
        .io_cfg.tx = GPIO_CAN1_TX,
        .io_cfg.rx = GPIO_CAN1_RX,
        .bit_timing.bitrate = CAN1_BITRATE,
        .tx_queue_depth = CAN1_TX_QUEUE_LENGTH,
    };
    stState = twai_new_node_onchip(&stCANNode1Config, &stCANBus1);
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN1 twai_new_node_onchip failed: %s", esp_err_to_name(stState));  
    }
    stState = twai_node_register_event_callbacks(stCANBus1, &stRxCallback, NULL);
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN1 failed to register callback: %s", esp_err_to_name(stState));  
    }
    stState = twai_node_enable(stCANBus1); 
    if ( stState != ESP_OK )
    {
        ESP_LOGE("CAN", "CAN1 Bus failed to start: %s", esp_err_to_name(stState));  
    }

    #endif

    /* Allocate Ring Buffer */
    xCANRingBuffer = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CAN_frame_t));
    if (xCANRingBuffer == NULL) {
        ESP_LOGE("CAN", "Failed to create CAN Queue");
        return ESP_ERR_NO_MEM;
    } 

    return stState;
}

esp_err_t CAN_transmit(twai_node_handle_t stCANBus, const CAN_frame_t *stFrame)
{
    /*
    *===========================================================================
    *   CAN_transmit
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    *            stFrame: CAN frame to transmit
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Transmits a CAN message on the given CAN bus. Uses a persistant memory pool to 
    *   store the message for some time to allow the CAN driver to copy out the data 
    *   before it is overwritten. 
    * 
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *   29/10/25 CP Updated to use onchip driver, old driver depriecated
    *   02/11/25 CP Makes transmit work with messages < 8 bytes
    *
    *===========================================================================
    */

    esp_err_t stState;
    can_tx_buffer_t *stCANTxBuffer = &astCANTxPool[byCANTxPoolIndex];
    boolean BExtendedID = FALSE;

    if (!stCANBus) 
    {
        ESP_LOGE("CAN", "CAN bus handle is NULL");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t byDLC = (uint8_t)stFrame->byDLC;
    if (byDLC > 8) 
    {
        ESP_LOGE("CAN", "Invalid CAN frame DLC: %u", byDLC);
        return ESP_ERR_INVALID_ARG;
    }

    /* Allocate memory location for this message in the pool. Wraps after MAX_CAN_TXS_PER_CALL. */
    can_tx_buffer_t *stTxBuf = &astCANTxPool[byCANTxPoolIndex];
    byCANTxPoolIndex++;
    if (byCANTxPoolIndex >= MAX_CAN_TXS_PER_CALL) 
    {
        byCANTxPoolIndex = 0;
    }
    stCANTxBuffer->dwData[0] = 0;
    stCANTxBuffer->dwData[1] = 0;
    
    if (byDLC > 0)
    {
        memcpy(stCANTxBuffer->dwData, stFrame->abData, byDLC);
    }

    if (stFrame->dwID > 0x7FF) 
    {
        BExtendedID = TRUE;
    }

    stCANTxBuffer->stFrame = (twai_frame_t)
    {
        .header.id  = (uint32_t)stFrame->dwID,
        .header.dlc = byDLC,
        .header.ide = BExtendedID,
        .header.rtr = FALSE,
        .header.fdf = FALSE,
        .header.brs = FALSE,
        .buffer     = (uint8_t *)stCANTxBuffer->dwData,
        .buffer_len = byDLC,
    };   

    stState = twai_node_transmit(stCANBus, &stCANTxBuffer->stFrame, FALSE);
    return stState;
}

esp_err_t CAN_receive_debug()
{   
    /*
    *===========================================================================
    *   CAN_receive_debug
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Empties the CAN ring buffer by logging all received messages to the console.
    * 
    *=========================================================================== 
    *   Revision History:
    *   29/10/25 CP Initial Version
    *   02/11/25 CP Improved terminal readability
    *   23/11/25 CP Changed to use FreeRTOS queue, refactored
    *
    *===========================================================================
    */

    CAN_frame_t stCANFrame;

    if (!xCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }
     
    while (xQueueReceive(xCANRingBuffer, &stCANFrame, 0) == pdTRUE) 
    {  
        LOG_CAN_FRAME(stCANFrame);
    }
    return ESP_OK;
}

const char* CAN_error_state_to_string(twai_error_state_t stState) {
    switch (stState) {
        case TWAI_ERROR_ACTIVE:  return "TWAI_ERROR_ACTIVE";
        case TWAI_ERROR_WARNING: return "TWAI_ERROR_WARNING";
        case TWAI_ERROR_PASSIVE: return "TWAI_ERROR_PASSIVE";
        case TWAI_ERROR_BUS_OFF: return "TWAI_ERROR_BUS_OFF";
        default:                 return "UNKNOWN_TWAI_ERROR_STATE";
    }
}

void CAN_bus_diagnosics()
{
    /*
    *===========================================================================
    *   CAN_bus_diagnosics
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    * 
    *   Returns: Nothing.
    * 
    *   Checks the status of the CAN bus and attempts to recover if there is an
    *   error.
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *
    *===========================================================================
    */

    twai_node_status_t stBusStatus;
    twai_node_record_t stBusStatistics;
    static twai_error_state_t stLastErrorState = TWAI_ERROR_ACTIVE;

    #ifdef GPIO_CAN0_TX
    /* Check if the CAN bus is in error state and recover */
    twai_node_get_info(stCANBus0, &stBusStatus, &stBusStatistics);
    /* Detect state change */
    if (stBusStatus.state != stLastErrorState) {
        ESP_LOGW("CAN", "CAN0 bus error state changed from %s to %s",
            CAN_error_state_to_string(stLastErrorState),
            CAN_error_state_to_string(stBusStatus.state));
        stLastErrorState = stBusStatus.state;
    }
    /* If bad error then restart bus */
    if (stBusStatus.state == TWAI_ERROR_BUS_OFF) 
    {
        ESP_LOGW("CAN", "Recovering bus 0 : %s", esp_err_to_name(twai_node_recover(stCANBus0))); 
    }
    #endif

    #ifdef GPIO_CAN1_TX
    /* Check if the CAN bus is in error state and recover */
    twai_node_get_info(stCANBus1, &stBusStatus, &stBusStatistics);
    /* Detect state change */
    if (stBusStatus.state != stLastErrorState) {
        ESP_LOGW("CAN", "CAN1 bus error state changed from %s to %s",
            CAN_error_state_to_string(stLastErrorState),
            CAN_error_state_to_string(stBusStatus.state));
        stLastErrorState = stBusStatus.state;
    }
    /* If bad error then restart bus */
    if (stBusStatus.state == TWAI_ERROR_BUS_OFF) 
    {
        ESP_LOGW("CAN", "Recovering bus 1: %s", esp_err_to_name(twai_node_recover(stCANBus1))); 
    }
    #endif
}

bool CAN_receive_callback(twai_node_handle_t stCANBus, const twai_rx_done_event_data_t *edata, void *stRxCallback)
{
    /*
    *===========================================================================
    *   CAN_receive_callback
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    *            edata: No idea read https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/twai.html
    *            stRxCallback: see above
    * 
    *   Returns: 1 if successful, 0 not.
    * 
    *   The callback for CAN Rx, adds the message to the ring buffer. 
    *   If the ring buffer is full it drops the message.
    * 
    *=========================================================================== 
    *   Revision History:
    *   20/04/25 CP Initial Version
    *   08/10/25 CP Updated to implement ring buffer
    *   30/10/25 CP Updated to use onchip driver, old driver depriecated
    *   16/11/25 CP Respond to Command message
    *   23/11/25 CP Changed to use FreeRTOS queue instead of ring buffer, refactored
    *   03/01/26 CP Added reflash over CAN functionality.
    *
    *===========================================================================
    */

    esp_err_t stState;
    CAN_frame_t stRxedFrame;
    uint8_t abyRxBuffer[8];
    twai_frame_t stRxFrame = {
        .buffer = abyRxBuffer,
        .buffer_len = sizeof(abyRxBuffer),
    };
    
    stState = twai_node_receive_from_isr(stCANBus, &stRxFrame);
    if (stState != ESP_OK) 
    {
        return FALSE;
    }

    CAN_CMD_response(stRxFrame);

    if (!xCANRingBuffer) 
    {
        return FALSE;
    }

    /* Copy frame into buffer */
    stRxedFrame.dwID = (dword)stRxFrame.header.id;
    stRxedFrame.byDLC = (byte)stRxFrame.header.dlc;
    if (stRxedFrame.byDLC > 0)
    {
        memcpy(stRxedFrame.abData, stRxFrame.buffer, stRxFrame.header.dlc);
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(xCANRingBuffer, &stRxedFrame, &xHigherPriorityTaskWoken) != pdTRUE) {
        /* Queue Full */
        dwNDroppedCANFrames++;
        return FALSE;
    }
    return TRUE;

}

bool CAN_receive_callback_no_queue(twai_node_handle_t stCANBus, const twai_rx_done_event_data_t *edata, void *stRxCallback)
{
    /*
    *===========================================================================
    *   CAN_receive_callback_no_queue
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    *            edata: No idea read https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/twai.html
    *            stRxCallback: see above
    * 
    *   Returns: 1 if successful, 0 not.
    * 
    *   The callback for CAN Rx when the ring buffer is not used. Only responds
    *   to command messages.
    * 
    *=========================================================================== 
    *   Revision History:
    *   16/11/25 CP Initial Version
    *   23/11/25 CP Changed to use FreeRTOS queue instead of ring buffer, refactored
    *
    *===========================================================================
    */

    esp_err_t stState;
    uint8_t abyRxBuffer[8];
    twai_frame_t stRxFrame = {
        .buffer = abyRxBuffer,
        .buffer_len = sizeof(abyRxBuffer),
    };
    
    stState = twai_node_receive_from_isr(stCANBus, &stRxFrame);
    if (stState != ESP_OK)
    {
        return FALSE;
    }

    CAN_CMD_response(stRxFrame);

    return TRUE;
}

esp_err_t CAN_empty_ESPNOW_buffer(twai_node_handle_t stCANBus)
{
    /*
    *===========================================================================
    *   CAN_empty_ESPNOW_buffer
    *   Takes:   stCANBus: Pointer to the CAN bus handle
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Empties the CAN ring buffer by transmitting messages until the buffer is
    *   empty or the max number of messages per call is reached.
    *=========================================================================== 
    *   Revision History:
    *   15/10/25 CP Initial Version
    *   23/11/25 CP Changed to use FreeRTOS queue, refactored
    *
    *===========================================================================
    */

    esp_err_t eStatus = ESP_OK;
    word wCounter = 0;
    CAN_frame_t stCANFrame;   

    if (!xESPNOWRingBuffer) 
    {
        ESP_LOGE("CAN", "ESPNOW ring buffer not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    /* Until the queue is empty or the max number of messages is reached, send CAN messages */ 
    while (wCounter < MAX_CAN_TXS_PER_CALL && 
            xQueueReceive(xESPNOWRingBuffer, &stCANFrame, 0) == pdTRUE) 
    {    
        eStatus = CAN_transmit(stCANBus, &stCANFrame);  
        if (eStatus != ESP_OK) 
        {
            ESP_LOGI("CAN", "Failed to transmit CAN frame : %s", esp_err_to_name(eStatus));
            return eStatus;
        }
        wCounter++;
    }
    
    return eStatus;
}

void CAN_CMD_response(twai_frame_t stRxFrame)
{
    /*
    *===========================================================================
    *   CAN_CMD_response
    *   Takes:   stRxFrame: The received CAN frame
    * 
    *   Returns: Nothing.
    * 
    *   Responds to a CAN command message.
    * 
    *=========================================================================== 
    *   Revision History:
    *   03/01/26 CP Initial Version
    *
    *===========================================================================
    */

    if (stRxFrame.header.id == CAN_CMD_ID)
    {
        switch (stRxFrame.buffer[0])
        {
            case eCMD_RESET:
                esp_restart();
                break;
            case eCMD_CLEAR_MINMAX:
                for (word wNCounter = 0; wNCounter < eTASK_TOTAL; wNCounter++)
                {
                    adwMaxTaskTime[wNCounter] = 0;
                }
                break;
            case eCMD_CLEAR_ERRORS:
                /* To be implemented */     
                break;
            case eCMD_REFLASH_MODE:
                if (stRxFrame.header.dlc < 6)
                {
                    /* Not enough data for reflash command, ignore */
                    break;
                }
                dwFirmwareSize = ((dword)stRxFrame.buffer[2] << 24) |
                                 ((dword)stRxFrame.buffer[3] << 16) |
                                 ((dword)stRxFrame.buffer[4] << 8)  |
                                 ((dword)stRxFrame.buffer[5]);
                set_device_mode(eREFLASH);
                break;
            case eCMD_NORMAL_MODE:
                set_device_mode(eNORMAL);
                break;
            default:
                /* Unknown command, ignore */
                break;
        }
    }
}

void CAN_clear_rx_buffer(void)
{
    /*
    *===========================================================================
    *   CAN_clear_rx_buffer
    *   Takes:   None
    * 
    *   Returns: None
    * 
    *   Clears the CAN receive ring buffer.
    *=========================================================================== 
    *   Revision History:
    *   03/01/26 CP Initial Version
    *
    *===========================================================================
    */
    if (xCANRingBuffer != NULL) {
        xQueueReset(xCANRingBuffer);
    }
}

esp_err_t CAN_Tx_killlevel(KillLevel_t eKillLevel, KillSource_t eKillSource)
{
    /*
    *===========================================================================
    *   CAN_Tx_killlevel
    *   Takes:   eKillLevel: The level of kill to send
    *            eKillSource: The source of the kill
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *  Transmits a kill level message on CAN bus 0.
    * ===========================================================================
    *   Revision History:
    *   09/01/26 CP Initial Version
    * 
    * ==========================================================================
    */
    esp_err_t eStatus;
    CAN_frame_t stCANFrame;
    stCANFrame.dwID = KILL_MSG_ID;
    stCANFrame.byDLC = 3;
    stCANFrame.abData[0] = (byte)eKillLevel;
    stCANFrame.abData[1] = (byte)eKillSource;
    stCANFrame.abData[2] = (byte)((stCANFrame.abData[0] + stCANFrame.abData[1]) % 256); // Checksum
    eStatus = CAN_transmit(stCANBus0, &stCANFrame);
    return eStatus;
}

esp_err_t CAN_read_from_buffer(void)
{
    /*
    *===========================================================================
    *   CAN_read_from_buffer
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Empties the CAN buffer and reads selected messages into internal variables.
    * ===========================================================================
    *   Revision History:
    *   19/03/26 CP Initial Version
    * 
    * ==========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    CAN_frame_t stCANFrame; 
    
    if (!xCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (uxQueueMessagesWaiting(xCANRingBuffer) == 0)
    {
        /* Buffer empty */
        return ESP_OK;
    }

    while (xQueueReceive(xCANRingBuffer, &stCANFrame, 0) == pdTRUE)
    {
        /* Look for relevent CAN frames */
        switch (stCANFrame.dwID)
        {
            /* Populate this with the CAN IDs for messages required for this device. Example below in comment */
            // case STATUSAPPS_ID:
            //     /* Process BMS Cell Voltages */
            //     {
            //         StatusAPPSRx(stCANFrame);
            //     }
            //     break;
            case ERPM_DUTY_VOLTAGE_ID:
            {
                /* Process ERPM Duty Cycle and Voltage */
                ERPM_DUTY_VOLTAGERx(stCANFrame);
                break;
            }
            case CELLSTATS1_ID:
            {
                /* Process BMS Cell Voltages */
                CellStats1Rx(stCANFrame);
                break;
            } 
            case STATUSAPPS_ID:
            {
                /* Process APPS Status */
                StatusAPPSRx(stCANFrame);
                break;
            }
            case DASHDATA_ID:
            {
                /* Process Dash Data */
                DashDataRx(stCANFrame);
                break;
            }
            default:
                /* Ignore other CAN frames */
                break;
        }
    }

    return eStatus;
}