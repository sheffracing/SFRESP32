/*
espnow.c
File contains the ESP now functions for a esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include "espnow.h"
#include "sfrtypes.h"

/* --------------------------- Local Types ----------------------------- */
typedef enum {
    EXAMPLE_ESPNOW_SEND_CB,
    EXAMPLE_ESPNOW_RECV_CB,
} espnow_event_id_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} espnow_event_send_cb_t;

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} espnow_event_recv_cb_t;

typedef union {
    espnow_event_send_cb_t send_cb;
    espnow_event_recv_cb_t recv_cb;
} espnow_event_info_t;

typedef struct {
    espnow_event_id_t id;
    espnow_event_info_t info;
} espnow_event_t;

/* --------------------------- Local Variables ------------------------ */
QueueHandle_t xESPNOWRingBuffer = NULL;

/* --------------------------- Global Variables ----------------------- */
/*
* MAC Adresses of my devices
* 1: 8C:BF:EA:CF:94:24 : {0x8C, 0xBF, 0xEA, 0xCF, 0x94, 0x24}
* 2: 8C:BF:EA:CF:90:34 : {0x8C, 0xBF, 0xEA, 0xCF, 0x90, 0x34}
* 3: 9C:9E:6E:77:AF:50 : {0x9C, 0x9E, 0x6E, 0x77, 0xAF, 0x50}
* 4: 
* 5: 98:A3:16:85:6C:EC : {0x98, 0xA3, 0x16, 0x85, 0x6C, 0xEC}
*/
uint8_t byMACAddress[6] = {0x98, 0xA3, 0x16, 0x85, 0x6C, 0xEC}; // Change to the MAC address of target device

/* --------------------------- Definitions ----------------------------- */
#define PACKED_FRAME_SIZE 11 // 2 bytes ID + 1 byte DLC + 8 bytes data
#define TX_ENABLE  // Comment out if TX undesired

/* --------------------------- Function prototypes --------------------- */
esp_err_t ESPNOW_init(void);
esp_err_t NVS_init(void);
esp_err_t ESPNOW_fill_buffer(const byte *abyData, byte byNDataLength);
esp_err_t ESPNOW_empty_buffer(void);
static void ESPNOW_tx_callback(const wifi_tx_info_t *tx_info, esp_now_send_status_t eStatus);
static void ESPNOW_rx_callback(const esp_now_recv_info_t *recv_info, const uint8_t *byData, int byNLength);


/* --------------------------- Functions ----------------------------- */
esp_err_t ESPNOW_init(void)
{
    /*
    *===========================================================================
    *   ESPNOW_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Installs the wifi driver and starts the esp now service.
    *=========================================================================== 
    *   Revision History:
    *   04/05/25 CP Initial Version
    *   23/11/25 CP Added FreeRTOS queue for Rxed CAN messages 
    *
    *===========================================================================
    */
    wifi_init_config_t stWifiConfig = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t eStatus;

    /* Initialise NVS */
    eStatus = NVS_init();
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("NVS", "Failed to start: %s", esp_err_to_name(eStatus));
        return eStatus;
    }

    /* Switch to External Antenna */
    gpio_set_level(3,0);
    gpio_set_level(14,1);
    
    /* Initialise Wifi */
    esp_netif_init();
    esp_event_loop_create_default();
    esp_wifi_init(&stWifiConfig);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_ps(WIFI_PS_NONE);
    eStatus = esp_wifi_start();
    if (eStatus != ESP_OK) {
        ESP_LOGE("WiFi", "Failed to start: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
    eStatus = esp_now_init();
    if (eStatus != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to start: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    
    /* Print this ESP's MAC Address */
    uint8_t abyThisESPMacAddr[6];
    esp_read_mac(abyThisESPMacAddr, ESP_MAC_WIFI_STA);
    ESP_LOGI("ESP-NOW", "ESP MAC Address: %02X:%02X:%02X:%02X:%02X:%02X", 
             abyThisESPMacAddr[0], abyThisESPMacAddr[1], abyThisESPMacAddr[2],
             abyThisESPMacAddr[3], abyThisESPMacAddr[4], abyThisESPMacAddr[5]);  

    #ifdef TX_ENABLE
    /* Add Peers */
    esp_now_peer_info_t stPeerInfo = {0};
    memcpy(stPeerInfo.peer_addr, byMACAddress, ESP_NOW_ETH_ALEN);
    stPeerInfo.channel = CONFIG_ESPNOW_CHANNEL;
    stPeerInfo.encrypt = false;
    eStatus = esp_now_add_peer(&stPeerInfo);
    if (eStatus != ESP_OK) {
        ESP_LOGE("ESP-NOW", "Failed to add peer: %s", esp_err_to_name(eStatus));
        return eStatus;
    }
    #endif

    /* Allocate Ring Buffer */
    xESPNOWRingBuffer = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CAN_frame_t));
    if (xESPNOWRingBuffer == NULL) {
        ESP_LOGE("ESP-NOW", "Failed to create ESPNOW Queue");
        return ESP_ERR_NO_MEM;
    }

    /* Register Callbacks */
    esp_now_register_send_cb(ESPNOW_tx_callback);
    esp_now_register_recv_cb(ESPNOW_rx_callback);

    return eStatus;
}

esp_err_t NVS_init(void)
{
    /*
    *===========================================================================
    *   NVS_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Installs the flash driver and starts the storage manager. Needed for the
    *   wifi driver to work for some reason.
    *=========================================================================== 
    *   Revision History:
    *   04/05/25 CP Initial Version
    *
    *===========================================================================
    */
    esp_err_t eStatus;
    eStatus = nvs_flash_init();
    /* If flash is full wipe it and retry */
    if (eStatus == ESP_ERR_NVS_NO_FREE_PAGES || eStatus == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        eStatus = nvs_flash_erase();
        if (eStatus != ESP_OK)
        {
            ESP_LOGE("NVS", "Failed to erase flash: %s", esp_err_to_name(eStatus));
            return eStatus;
        }
        eStatus = nvs_flash_init();
    }
    return eStatus;
}

static void ESPNOW_tx_callback(const wifi_tx_info_t *tx_info, esp_now_send_status_t eStatus)
{
    /*
    *===========================================================================
    *   ESPNOW_tx_callback
    *   Takes:   eStatus - status of send
    *            tx_info - information about the transmission
    * 
    *   Returns: None
    * 
    *   The callback function for when data is sent via esp now. Currently does
    *   nothing. Do not do anything lengthy in this function.
    *=========================================================================== 
    *   Revision History:
    *   06/10/25 CP Initial Version
    *
    *===========================================================================
    */

}

static void ESPNOW_rx_callback(const esp_now_recv_info_t *recv_info, const uint8_t *byData, int byNLength)
{
     /*
    *===========================================================================
    *   ESPNOW_rx_callback
    *   Takes:   recv_info - information about the received data
    *            byData - pointer to the received data
    *            byNLength - length of the received data
    * 
    *   Returns: None
    * 
    *   The callback function for when data is received via esp now. When a packet
    *   is Rxed add it to a queue for processing. Do not do anything lengthy in 
    *   this function, post to a queue
    *   and handle it from a lower priority task.
    *=========================================================================== 
    *   Revision History:
    *   06/10/25 CP Initial Version
    *
    *===========================================================================
    */

    ESPNOW_fill_buffer(byData, byNLength);

}

esp_err_t ESPNOW_empty_buffer(void)
{
    /*
    *===========================================================================
    *   ESPNOW_empty_buffer
    *   Takes:   None
    * 
    *   Returns: eStatus - ESP_OK if successful, error code if not.
    * 
    *   Empties the CAN ring buffer by packing as many CAN frames as possible
    *   into a single ESP-NOW packet (250 bytes) and sending it. If there are no
    *   frames to send, it returns ESP_OK. Each CAN frame takes 11 bytes in the
    *   ESP-NOW packet (2 bytes ID, 1 byte DLC, 8 bytes data). The ring buffer is
    *   115 frames in total so it can take up to 3 ESP-NOW packets to empty
    *   the buffer if it is full. This function only sends one ESP-NOW packet per
    *   call, it is intended to be run once per 100ms or so.
    * 
    *=========================================================================== 
    *   Revision History:
    *   08/10/25 CP Initial Version
    *   23/11/25 CP Changed to use FreeRTOS queue instead of ring buffer, refactored
    *
    *===========================================================================
    */

    byte byBytesToSend[MAX_ESPNOW_PAYLOAD];
    dword dwOffset = 0;
    CAN_frame_t stCANFrame;

    for (byte NLoopCounter = 0; NLoopCounter < MAX_ESPNOW_PAYLOAD; NLoopCounter++) 
    {
        byBytesToSend[NLoopCounter] = 0;
    }

    if (!xCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    } 

    /* Until the ring buffer is empty or the ESP-NOW message is full, pack the message */ 
    while (dwOffset + PACKED_FRAME_SIZE <= MAX_ESPNOW_PAYLOAD && xQueueReceive(xCANRingBuffer, &stCANFrame, 0) == pdTRUE)
    {
        uint8_t byDLC = stCANFrame.byDLC;
        if (byDLC > 8 || byDLC < 1) 
        {
            ESP_LOGE("ESP-NOW", "Invalid CAN frame DLC: %u", byDLC);
            break;
        }

        byBytesToSend[dwOffset + 0] = (byte)((stCANFrame.dwID >> 8) & 0xFF);
        byBytesToSend[dwOffset + 1] = (byte)(stCANFrame.dwID & 0xFF);
        byBytesToSend[dwOffset + 2] = (byte)byDLC;
        memcpy(&byBytesToSend[dwOffset + 3], stCANFrame.abData, byDLC);
        dwOffset += 3 + byDLC; // 2 bytes ID + 1 byte DLC + 1-8 bytes data
    }

    /* If data is present send it otherwise return ESP_OK */
    if (dwOffset > 0) 
    {
        return esp_now_send(byMACAddress, byBytesToSend, dwOffset);
    }
    
    return ESP_OK;
}

esp_err_t ESPNOW_fill_buffer(const byte *abyData, byte byNDataLength)
{
    /*
    *===========================================================================
    *   ESPNOW_fill_buffer
    *   Takes:   abData - pointer to data Rxed over ESP-NOW
    *            byNDataLength - length of data Rxed
    * 
    *   Returns: None
    * 
    *   Processes data received over ESP-NOW and adds it to the CAN ring buffer.
    *   The ring buffer is intended to be emptied by a seperate CAN task. If the
    *   buffer is full (or not initialised) the message will be dropped.
    * 
    *=========================================================================== 
    *   Revision History:
    *   15/10/25 CP Initial Version
    *   03/11/25 CP Fixed the way this was writing to the ring buffer, god what a nightmare
    *   23/11/25 CP Changed to use FreeRTOS queue instead of ring buffer, refactored
    *
    *===========================================================================
    */

    volatile uint16_t wOffset = 0;
    CAN_frame_t stFrame;

    if (byNDataLength <= 0) 
    {
        return ESP_OK;
    }

    if (!xESPNOWRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }

    while (wOffset < byNDataLength) {
        /* Ensure at least ID(2) + DLC(1) remain */
        if (byNDataLength - wOffset < 3) break;
 
        memset(&stFrame, 0, sizeof(CAN_frame_t));
        stFrame.dwID = ((uint32_t)abyData[wOffset] << 8)  |
                        ((uint32_t)abyData[wOffset + 1]);
        wOffset += 2;
        stFrame.byDLC = abyData[wOffset];
        wOffset += 1;
        
        if (stFrame.byDLC > 8) 
        {
            //Invalid DLC, drop frame
            break;
        }
        if (byNDataLength - wOffset < stFrame.byDLC) {
            /* Truncated frame, drop frame */
            break;
        }

        if (stFrame.byDLC > 0)
        {
            memcpy(stFrame.abData, &abyData[wOffset], stFrame.byDLC);
            wOffset += stFrame.byDLC;
        }

        if (xQueueSend(xESPNOWRingBuffer, &stFrame, 0) != pdTRUE) 
        {
            /* Buffer full, drop frames */
            break;
        }
    }
    return ESP_OK;
}
