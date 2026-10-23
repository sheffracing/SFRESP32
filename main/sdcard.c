/*
sdcard.c
File contains the SDCard functions for a esp32c6.

Written by Cole Perera for Sheffield Formula Racing 2025
This software was written by humans and is licensed under GNU General Public License v3.0. See LICENSE.txt for more details.
*/

#include "sdcard.h"

/* --------------------------- Global Variables ----------------------------- */
static const char *SD_MOUNT_POINT = "/sdcard";
static char abyFilePath[64] = "/sdcard/log000.bin";

/* --------------------------- Local Variables ------------------------------ */
extern dword dwTimeSincePowerUpms;
FILE *stFile;

/* --------------------------- Function prototypes -------------------------- */
esp_err_t SD_card_init(void);
esp_err_t sdcard_empty_buffer(void);

/* --------------------------- Definitions ---------------------------------- */
#define MAX_FILES 5
#define ALLOCATION_UNIT_SIZE 16 * 1024
#define SDMMC_FREQ 10000 // 10 kHz
#define MAX_TRANSFER_SIZE 4000 // max transfer size of one spi operation (bytes)
#define WRITE_BUFFER_SIZE 16384 // 16KB Buffer
#define FILE_SPEC_VERSION 0x01
#define MAX_WRITES_PER_CALL 50

typedef struct __attribute__((packed)) {
    byte     type;
    uint32_t dwTimestamp;
    uint16_t dwID;
    uint8_t  byDLC;
    uint8_t  abData[8];
} BinLogEntry_t;

typedef enum {
    CAN = 0x01,
    CAN_FD,
    CAN_STATE,
    ETERNAL_CLOCK,
} eLogEntryType_t;

/* --------------------------- Functions ------------------------------------ */

esp_err_t SD_card_init(void)
{
    /*
    *===========================================================================
    *   SD_card_init
    *   Takes:   None
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Initializes the SD card interface.
    *===========================================================================
    *   Revision History:
    *   20/10/25 CP Initial Version
    *   25/11/25 CP Switch to asc format
    *
    *===========================================================================
    */
    esp_err_t eStatus = ESP_OK;
    esp_vfs_fat_sdmmc_mount_config_t stSDMountConfig = 
    {
        .format_if_mount_failed = true,
        .max_files = MAX_FILES,
        .allocation_unit_size = ALLOCATION_UNIT_SIZE
    };
    sdmmc_card_t *stSDCard;
    sdmmc_host_t stSDCardHost = SDSPI_HOST_DEFAULT();
    stSDCardHost.max_freq_khz = SDMMC_FREQ;
    sdspi_device_config_t stSDCardSlot = SDSPI_DEVICE_CONFIG_DEFAULT();
    stSDCardSlot.gpio_cs = SPI_SD_CS;
    stSDCardSlot.host_id = stSDCardHost.slot;

    ESP_LOGI("SDCARD", "Initializing SD card...");
    if (eStatus != ESP_OK)
    {
        ESP_LOGE("SDCARD", "Failed to initialize SPI bus: %s", esp_err_to_name(eStatus));
        return eStatus;
    }

    ESP_LOGI("SDCARD", "Mounting filesystem...");
    eStatus = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &stSDCardHost, &stSDCardSlot, &stSDMountConfig, &stSDCard);
    if (eStatus != ESP_OK)
    {
        if (eStatus == ESP_FAIL)
        {
            ESP_LOGE("SDCARD", "Failed to mount filesystem. Formatting...");
            eStatus = esp_vfs_fat_sdcard_format("/sdcard", stSDCard);
            if (eStatus != ESP_OK)
            {
                ESP_LOGE("SDCARD", "Failed to format: %s", esp_err_to_name(eStatus));
                return eStatus;
            }
        }
        else
        {
            ESP_LOGE("SDCARD", "Failed to mount filesystem: %s", esp_err_to_name(eStatus));
            return eStatus;
        }
    }
    ESP_LOGI("SDCARD", "Mounted successfully.");
    /* Print Card Details */
    sdmmc_card_print_info(stdout, stSDCard);
    /* Query card to update file abyFilePath */
    DIR *stDirectory;
    stDirectory = opendir(SD_MOUNT_POINT);
    if (stDirectory == NULL)
    {
        ESP_LOGE("SDCARD", "Failed to open directory");
        return ESP_ERR_INVALID_STATE;
    }
    struct dirent *stDirInfo;
    stDirInfo = readdir(stDirectory);
    word wNLastFile = 0;
    while (stDirInfo != NULL)
    {
        wNLastFile++;
        stDirInfo = readdir(stDirectory);
    }
    closedir(stDirectory);
    snprintf(abyFilePath, sizeof(abyFilePath), "%s/log%03d.bin", SD_MOUNT_POINT, (int)wNLastFile);
    stFile = fopen(abyFilePath, "ab");
    
    if (stFile != NULL)
    {
        static char acBuffer[WRITE_BUFFER_SIZE];
        setvbuf(stFile, acBuffer, _IOFBF, sizeof(acBuffer));
    }

    byte byFileSpecVersion = FILE_SPEC_VERSION;
    byte abyTimeData[6] = {0};
    byte abReserved[9] = {0};
    fwrite(&byFileSpecVersion, sizeof(byFileSpecVersion), 1, stFile);
    fwrite(&abyTimeData, sizeof(abyTimeData), 1, stFile);
    fwrite(&abReserved, sizeof(abReserved), 1, stFile);

   return eStatus;
}

esp_err_t sdcard_empty_buffer(void)
{
    /*
    *===========================================================================
    *   sdcard_empty_buffer
    *   Takes:   None
    * 
    *   Returns: eStatus - ESP_OK if successful, error code if not.
    * 
    *   Empties the CAN ring buffer dumping the contents into the sdcard. If there 
    *   is no data to append, it returns ESP_OK. The ring buffer is
    *   115 frames in total.
    * 
    *=========================================================================== 
    *   Revision History:
    *   24/10/25 CP Initial Version
    *   25/11/25 CP Changed to use FreeRTOS queue
    *   26/11/25 CP Switch to asc format
    *   27/11/25 CP Switch to binary format
    *
    *===========================================================================
    */

    CAN_frame_t stCANFrame; 
    BinLogEntry_t stLogEntry;
    word wNWrites = 0;

    if (!xCANRingBuffer) 
    {
        return ESP_ERR_INVALID_STATE;
    }
   
    if (uxQueueMessagesWaiting(xCANRingBuffer) == 0)
    {
        return ESP_OK;
    }

    if (stFile == NULL)
    {
        return ESP_FAIL;
    }

    /* Until the ring buffer is empty, write lines to file */ 
    while (xQueueReceive(xCANRingBuffer, &stCANFrame, 0) == pdTRUE
           && wNWrites < MAX_WRITES_PER_CALL)
    {
        wNWrites++;
        stLogEntry.type = CAN;
        stLogEntry.dwTimestamp = dwTimeSincePowerUpms;
        stLogEntry.dwID = stCANFrame.dwID;
        stLogEntry.byDLC = stCANFrame.byDLC;
        memcpy(stLogEntry.abData, stCANFrame.abData, 8);
        fwrite(&stLogEntry, sizeof(BinLogEntry_t), 1, stFile);
    }
    return ESP_OK;
}