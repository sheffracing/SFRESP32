/*
tasks.c
File contains the tasks that run on the device. Tasks are called by interrupts at their specified rates.
Tasks are:
    task_BG: Background task that runs as often as processor time is available.
    task_1ms: Task that runs every 1ms.
    task_100ms: Task that runs every 100ms.

Written by Cole Perera and Aditya Parnandi for Sheffield Formula Racing 2025
This software was written by humans and is licensed under GNU General Public License v3.0. See LICENSE.txt for more details.
*/
#include "tasks.h"

/* --------------------------- Local Types ----------------------------- */


/* --------------------------- Local Variables ----------------------------- */ 
extern uint8_t byMACAddress[6];
extern esp_reset_reason_t eResetReason;
extern eChipMode_t eDeviceMode;
static esp_partition_t *stOTAPartition = NULL;
extern stADCHandles_t stADCHandle0;
extern stADCHandles_t stADCHandle1;

stSensorMap_t stAPPS1Map = {
    .fLowerLimit = 0.5f,
    .fUpperLimit = 3.1f,
    .afLookupTable = {
        {0.0f, 0.5f, 0.57f, 0.60f, 0.62f, 0.65f, 0.68f, 0.70f, 0.72f, 0.75f,
        0.78f, 0.80f, 0.82f, 0.85f, 0.88f, 0.90f, 0.93f, 0.95f, 0.98f, 1.00f,
        1.02f, 1.05f, 1.08f, 1.10f, 1.12f, 1.15f, 1.18f, 1.20f, 1.23f, 1.25f,
        1.27f, 1.30f, 1.33f, 1.35f, 1.38f, 1.40f, 1.43f, 1.45f, 1.48f, 1.50f,
        1.53f, 1.55f, 1.57f, 1.60f, 1.62f, 1.65f, 1.68f, 1.70f, 1.73f, 1.75f,
        1.78f, 1.80f, 1.83f, 1.85f, 1.88f, 1.90f, 1.93f, 1.95f, 1.98f, 2.00f,
        2.03f, 2.05f, 2.08f, 2.10f, 2.12f, 2.15f, 2.17f, 2.20f, 2.23f, 2.25f,
        2.28f, 2.30f, 2.33f, 2.35f, 2.38f, 2.40f, 2.42f, 2.45f, 2.48f, 2.50f,
        2.52f, 2.55f, 2.58f, 2.60f, 2.62f, 2.65f, 2.68f, 2.70f, 2.73f, 2.75f,
        2.77f, 2.80f, 2.83f, 2.85f, 2.88f, 2.90f, 2.93f, 2.95f, 2.98f, 3.10f, 5.0f},
        {00.0f, 01.0f, 02.0f, 03.0f, 04.0f, 05.0f, 06.0f, 07.0f, 08.0f, 09.0f, 
        10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f, 19.0f, 
        20.0f, 21.0f, 22.0f, 23.0f, 24.0f, 25.0f, 26.0f, 27.0f, 28.0f, 29.0f, 
        30.0f, 31.0f, 32.0f, 33.0f, 34.0f, 35.0f, 36.0f, 37.0f, 38.0f, 39.0f, 
        40.0f, 41.0f, 42.0f, 43.0f, 44.0f, 45.0f, 46.0f, 47.0f, 48.0f, 49.0f, 
        50.0f, 51.0f, 52.0f, 53.0f, 54.0f, 55.0f, 56.0f, 57.0f, 58.0f, 59.0f, 
        60.0f, 61.0f, 62.0f, 63.0f, 64.0f, 65.0f, 66.0f, 67.0f, 68.0f, 69.0f, 
        70.0f, 71.0f, 72.0f, 73.0f, 74.0f, 75.0f, 76.0f, 77.0f, 78.0f, 79.0f, 
        80.0f, 81.0f, 82.0f, 83.0f, 84.0f, 85.0f, 86.0f, 87.0f, 88.0f, 89.0f, 
        90.0f, 91.0f, 92.0f, 93.0f, 94.0f, 95.0f, 96.0f, 97.0f, 98.0f, 99.0f, 100.0f}},
};

stSensorMap_t stAPPS2Map = {
    .fLowerLimit = 0.5f,
    .fUpperLimit = 3.1f,
    .afLookupTable = {
        {0.0f, 0.5f, 0.57f, 0.60f, 0.62f, 0.65f, 0.68f, 0.70f, 0.72f, 0.75f,
        0.78f, 0.80f, 0.82f, 0.85f, 0.88f, 0.90f, 0.93f, 0.95f, 0.98f, 1.00f,
        1.02f, 1.05f, 1.08f, 1.10f, 1.12f, 1.15f, 1.18f, 1.20f, 1.23f, 1.25f,
        1.27f, 1.30f, 1.33f, 1.35f, 1.38f, 1.40f, 1.43f, 1.45f, 1.48f, 1.50f,
        1.53f, 1.55f, 1.57f, 1.60f, 1.62f, 1.65f, 1.68f, 1.70f, 1.73f, 1.75f,
        1.78f, 1.80f, 1.83f, 1.85f, 1.88f, 1.90f, 1.93f, 1.95f, 1.98f, 2.00f,
        2.03f, 2.05f, 2.08f, 2.10f, 2.12f, 2.15f, 2.17f, 2.20f, 2.23f, 2.25f,
        2.28f, 2.30f, 2.33f, 2.35f, 2.38f, 2.40f, 2.42f, 2.45f, 2.48f, 2.50f,
        2.52f, 2.55f, 2.58f, 2.60f, 2.62f, 2.65f, 2.68f, 2.70f, 2.73f, 2.75f,
        2.77f, 2.80f, 2.83f, 2.85f, 2.88f, 2.90f, 2.93f, 2.95f, 2.98f, 3.10f, 5.0f},
        {100.00, 98.99, 97.98, 96.97, 95.96, 94.95, 93.94, 92.93, 91.92, 90.91, 
        89.90, 88.89, 87.88, 86.87, 85.86, 84.85, 83.84, 82.83, 81.82, 80.81, 
        79.80, 78.79, 77.78, 76.77, 75.76, 74.75, 73.74, 72.73, 71.72, 70.71, 
        69.70, 68.69, 67.68, 66.67, 65.66, 64.65, 63.64, 62.63, 61.62, 60.61, 
        59.60, 58.59, 57.58, 56.57, 55.56, 54.55, 53.54, 52.53, 51.52, 50.51, 
        49.49, 48.48, 47.47, 46.46, 45.45, 44.44, 43.43, 42.42, 41.41, 40.40, 
        39.39, 38.38, 37.37, 36.36, 35.35, 34.34, 33.33, 32.32, 31.31, 30.30, 
        29.29, 28.28, 27.27, 26.26, 25.25, 24.24, 23.23, 22.22, 21.21, 20.20, 
        19.19, 18.18, 17.17, 16.16, 15.15, 14.14, 13.13, 12.12, 11.11, 10.10, 
        09.09, 08.08, 07.07, 06.06, 05.05, 04.04, 03.03, 02.02, 01.01, 00.00,}},
};

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

    /* CAN error handling */
    CANRxCheck1ms();

    /* Read Sensor 1 */
    rAPPs[0] = read_sensor(&stADCHandle0, &stAPPS1Map);
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
    rAPPs[1] = read_sensor(&stADCHandle1, &stAPPS1Map);
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

    /* CAN error handling */
    CANRxCheck1ms();

    /* Status and Drive enable messages */
    SetDriveEnableTx(stCANBus0);
    StatusAPPSSensorTx(stCANBus0);

    /* Every Second */
    if ( wNCounter % (PERIOD_1S / PERIOD_TASK_100MS) == 0 ) 
    {
        /* Toggle LED */
        pin_toggle(GPIO_ONBOARD_LED); 

        /* Send Status Message */
        /* This is not how you should send a CAN message but in this special case it is better this way */
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
