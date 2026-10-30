/*
tasks.c
File contains the tasks that run on the device. Tasks are called by interrupts at their specified rates.
Tasks are:
    task_BG: Background task that runs as often as processor time is available.
    task_1ms: Task that runs every 1ms.
    task_100ms: Task that runs every 100ms.

Written by Cole Perera and Aditya Parnandi for Sheffield Formula Racing 2025
*/
#include "tasks.h"

/* --------------------------- Local Types ----------------------------- */
#define APPS1_RAW_0_PCT   1007.0f
#define APPS1_RAW_100_PCT  966.0f
#define APPS2_RAW_0_PCT   2235.0f
#define APPS2_RAW_100_PCT 2265.0f

/* --------------------------- Local Variables ----------------------------- */ 
extern uint8_t byMACAddress[6];
extern esp_reset_reason_t eResetReason;
extern eChipMode_t eDeviceMode;
static esp_partition_t *stOTAPartition = NULL;
extern stADCHandles_t stADCHandle0;
extern stADCHandles_t stADCHandle1;

/* --------------------------- Global Variables ----------------------------- */
dword adwMaxTaskTime[eTASK_TOTAL];
dword adwLastTaskTime[eTASK_TOTAL];
eTaskState_t astTaskState[eTASK_TOTAL];
dword dwTimeSincePowerUpms = 0;

/* --------------------------- Definitions ----------------------------- */
#define PERIOD_TASK_100MS 100   // ms
#define PERIOD_10S 10000        // ms
#define PERIOD_1S 1000          // ms
#define MAX_eREFLASH_TIME_US 300000000 // us
#define TIME_BETWEEN_CAN_MSGS 100   // ms
#define APPS_MAX_DIFFERENCE 5       // Maximum difference between APPS sensors (%)

/* --------------------------- Functions ----------------------------- */
/* Background task that runs as often as processor time is available. */
void task_BG(void)
{ 
    static qword qwtTaskTimer;
    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_BG] = eTASK_ACTIVE;

    /* Service the watchdog if all task have been completed at least once */
    word wNTaskCounter = 0;
    boolean bTasksComplete = TRUE;
    for (wNTaskCounter = 0; wNTaskCounter < eTASK_TOTAL; wNTaskCounter++)
    {
        if (astTaskState[wNTaskCounter] == eTASK_INACTIVE) {
            bTasksComplete = FALSE;
            break;
        }
    }
    if (bTasksComplete) {
        /* Reset the watchdog timer */
        (void)esp_task_wdt_reset();           
        
        for (wNTaskCounter = 0; wNTaskCounter < eTASK_TOTAL; wNTaskCounter++)
        {
            astTaskState[wNTaskCounter] = eTASK_INACTIVE;
        }
    }

    /* Update max task time */
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    adwLastTaskTime[eTASK_BG] = (dword)qwtTaskTimer;
    if (qwtTaskTimer > adwMaxTaskTime[eTASK_BG]) {
        adwMaxTaskTime[eTASK_BG] = (dword)qwtTaskTimer;
    }
}

/* Task that runs every 1ms. */
void task_1ms(void)
{
    qword qwtTaskTimer;
    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_1MS] = eTASK_ACTIVE;

    /* Read Sensor 1 */
    rAPPs[0] = adc_read_raw_to_percentage(&stADCHandle0, APPS1_RAW_0_PCT, APPS1_RAW_100_PCT);
    if (rAPPs[0] == -999.0f)
    {
        /* Sensor Error */
        BAPPSFail[0] = TRUE;
    }
    else
    {
        BAPPSFail[0] = FALSE;
    }

    /* Read Sensor 2 */
    rAPPs[1] = adc_read_raw_to_percentage(&stADCHandle1, APPS2_RAW_0_PCT, APPS2_RAW_100_PCT);
    if (rAPPs[1] == -999.0f)
    {
        /* Sensor Error */
        BAPPSFail[1] = TRUE;
    }
    else
    {
        BAPPSFail[1] = FALSE;
    }

    /* This actually checkes if drift is > diff + 1 because of int cast */
    BAPPSDrift = (abs(rAPPs[0] - rAPPs[1]) > APPS_MAX_DIFFERENCE) ? TRUE : FALSE;
    BThrottleOK = !(BAPPSFail[0] || BAPPSFail[1] || BAPPSDrift);

    /* Calculate and send throttle if checks pass */
    if (BThrottleOK)
    {
        rAPPsFinal = (rAPPs[0] + rAPPs[1]) / 2.0f;
        CMD_TargetRelativeCurrent = rAPPsFinal;
        CMD_DriveEnable = TRUE;
        SetRelCurrentTx(stCANBus0);
    } else
    {
        CMD_DriveEnable = FALSE;
        rAPPsFinal = 0.0f;
    }

    /* Update time since power up */
    dwTimeSincePowerUpms++;

    /* Update max task time */
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    adwLastTaskTime[eTASK_1MS] = (dword)qwtTaskTimer;
    if (qwtTaskTimer > adwMaxTaskTime[eTASK_1MS]) 
    {
        adwMaxTaskTime[eTASK_1MS] = (dword)qwtTaskTimer;
    }
}

/* Task that runs every 100ms. */
void task_100ms(void)
{
    
    static qword qwtTaskTimer;
    static word wNCounter;

    qwtTaskTimer = esp_timer_get_time();
    astTaskState[eTASK_100MS] = eTASK_ACTIVE;

    /* Status and Drive enable messages */
    SetDriveEnableTx(stCANBus0);
    StatusAPPSSensorTx(stCANBus0);

    /* Every Second */
    if ( wNCounter % (PERIOD_1S / PERIOD_TASK_100MS) == 0 ) 
    {
        /* Toggle LED */
        pin_toggle(GPIO_ONBOARD_LED); 

        /* MCU Status Message */
        CAN_transmit(stCANBus0, &(CAN_frame_t)
        {
            .dwID = DEVICE_ID,
            .byDLC = 8,
            .abData = {
                (byte)(adwLastTaskTime[eTASK_1MS] / 50 & 0xFF),          
                (byte)(adwMaxTaskTime[eTASK_1MS] / 50 & 0xFF),
                (byte)(adwLastTaskTime[eTASK_100MS] / 500 & 0xFF),       
                (byte)(adwMaxTaskTime[eTASK_100MS] / 500 & 0xFF),
                (byte)(adwLastTaskTime[eTASK_BG] / 500 & 0xFF),        
                (byte)(adwMaxTaskTime[eTASK_BG] / 500 & 0xFF),
                (byte)((dwTimeSincePowerUpms/4000) >> 4 & 0xFF),
                (byte)(((dwTimeSincePowerUpms/4000) & 0xF) << 4 | (eResetReason & 0x0F)),
            }
        });

        /* Debug */
        ESP_LOGI(SFR_TAG, "Sensor Values: %f, %f, Final: %f, Throttle OK: %s", 
            rAPPs[0], rAPPs[1], rAPPsFinal, BThrottleOK ? "Yes" : "No");

    };

    /* Every 10 Seconds */
    if (wNCounter >= PERIOD_10S / PERIOD_TASK_100MS)
    {
        /* Print the task times */
        #ifdef DEBUG
        ESP_LOGI(SFR_TAG, "Max Task Time: %5d BG %5d 1ms %5d 100ms", 
            (int)adwMaxTaskTime[eTASK_BG],
            (int)adwMaxTaskTime[eTASK_1MS],
            (int)adwMaxTaskTime[eTASK_100MS]);
        ESP_LOGI(SFR_TAG, "Last Task Time: %5d BG %5d 1ms %5d 100ms", 
            (int)adwLastTaskTime[eTASK_BG], 
            (int)adwLastTaskTime[eTASK_1MS],
            (int)adwLastTaskTime[eTASK_100MS]);
        wNCounter = 0;
        #endif
    }
    wNCounter++;

    /* Check if the CAN bus is in error state and recover */
    CAN_bus_diagnosics();

    /* Update max task time */
    qwtTaskTimer = esp_timer_get_time() - qwtTaskTimer;
    adwLastTaskTime[eTASK_100MS] = (dword)qwtTaskTimer;
    if (qwtTaskTimer > adwMaxTaskTime[eTASK_100MS]) 
    {
        adwMaxTaskTime[eTASK_100MS] = (dword)qwtTaskTimer;
    }
}

/* Task that runs every 1ms in reflash mode */
void reflash_task_1ms(void)
{

}

/* Task that runs every 100ms in reflash mode */
void reflash_task_100ms(void)
{
    pin_toggle(GPIO_ONBOARD_LED);
}

/* Background task for reflash mode */
void reflash_task_BG()
{
    static qword qwtReflashEntryTime = 0;
    esp_err_t eState;
    (void)esp_task_wdt_reset();

    /* Initialise Reflash Mode */
    if (qwtReflashEntryTime == 0)
    {
        ESP_LOGI("CANFLASH", "Entering reflash mode with firmware size %d bytes", dwFirmwareSize);
        qwtReflashEntryTime = (qword)esp_timer_get_time();
    }

    if (stOTAPartition == NULL)
    {
        stOTAPartition = (esp_partition_t *)esp_ota_get_next_update_partition(NULL);
        if (stOTAPartition == NULL)
        {
            ESP_LOGE("CANFLASH", "Failed to find OTA partition");
        } else 
        {
            ESP_LOGI("CANFLASH", "OTA partition found at address 0x%08X, size %d bytes",
                stOTAPartition->address, stOTAPartition->size);
            eState = esp_partition_erase_range(stOTAPartition, 0, stOTAPartition->size);
            if (eState != ESP_OK) {
                ESP_LOGE("CANFLASH", "Failed to erase OTA partition: %s", esp_err_to_name(eState));
                stOTAPartition = NULL;
            }
        }
    }

    /* Write new binary as its recieved */
    eState = CAN_flash_empty_queue(stOTAPartition);
    if (eState != ESP_OK)
    {
        ESP_LOGE("CANFLASH", "Failed to read reflash data to flash: %s", esp_err_to_name(eState));
    }
    eState = CAN_flash_write(stOTAPartition);
    if (eState != ESP_OK)
    {
        ESP_LOGE("CANFLASH", "Failed to write reflash data to flash: %s", esp_err_to_name(eState));
    }

    /* If binary fully received then restart */
    if (dwBytesWrittenReflash >= dwFirmwareSize && dwFirmwareSize > 0)
    {
        uint32_t dwFlashTime = (uint32_t)((esp_timer_get_time() - qwtReflashEntryTime)/1000000);
        ESP_LOGI("CANFLASH", "Reflash complete, written %d bytes in %d s", (int)dwBytesWrittenReflash, dwFlashTime);
        eState = esp_ota_set_boot_partition(stOTAPartition);
        if (eState == ESP_OK) {
            esp_restart();
        }
        ESP_LOGE("CANFLASH", "Failed to set boot partition: %s", esp_err_to_name(eState));
    }
}

void pin_toggle(gpio_num_t ePin)
{
    static boolean BLEDState = false;
    BLEDState = !BLEDState;
    gpio_set_level(ePin, BLEDState);
}
