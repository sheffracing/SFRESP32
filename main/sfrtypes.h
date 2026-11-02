/*
sfrtypes.h | Sheffield Formula Racing
Generic types for SFR codebase

Written by Cole Perera for Sheffield Formula Racing 2025
*/
#define DEBUG
#ifndef SFRTypes
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define DEVICE_ID 0xF5 // UPDATE THIS FOR EACH DEVICE

#define TRUE 1
#define FALSE 0

typedef int boolean;

typedef unsigned char byte;
typedef signed char sbyte;

typedef unsigned short word;
typedef signed short sword;

typedef unsigned long dword;
typedef signed long sdword;

typedef unsigned long long qword;
typedef signed long long sqword;

typedef struct __attribute__((aligned(4))) {
    uint32_t dwID;       // 4 bytes
    uint8_t  byDLC;      // 0-8 (Data Length Code)
    uint8_t  abData[8];  // 8 bytes
    uint8_t  padding[3]; // 3 bytes explicit padding to reach 16 bytes total
} CAN_frame_t;

_Static_assert(sizeof(CAN_frame_t) == 16, "CAN_frame_t size mismatch!");

typedef struct {
    adc_cali_handle_t stCalibration;
    adc_oneshot_unit_handle_t stADCUnit;
    adc_channel_t eNChannel;
} stADCHandles_t;

typedef struct {
    float fLowerLimit;
    float fUpperLimit;
    float afLookupTable[2][101];
} stSensorMap_t;

typedef enum {
    eTASK_BG = 0,
    eTASK_1MS,
    eTASK_100MS,
    eTASK_TOTAL,
} eTasks_t;

typedef enum {
    eTASK_ACTIVE = 0,
    eTASK_INACTIVE,
} eTaskState_t;

typedef enum {
    eREFLASH =  0,
    eNORMAL
} eChipMode_t;

#define LOG_CAN_FRAME(frame) do { \
    char _buf[128]; \
    int _off = snprintf(_buf, sizeof(_buf), "ID=%u DLC=%u", (unsigned)(frame).dwID, (unsigned)(frame).byDLC); \
    for (int _i = 0; _i < (frame).byDLC && _i < 8; _i++) \
        _off += snprintf(_buf + _off, sizeof(_buf) - _off, " %02X", (unsigned)(frame).abData[_i]); \
    ESP_LOGI("CAN", "%s", _buf); \
} while(0)

#define SFRTypes
#endif