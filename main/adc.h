#ifndef ADC_H
#define ADC_H

#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "pin.h"
#include "main.h"

/* --------------------------- Function prototypes --------------------- */
esp_err_t adc_register(adc_atten_t eNAtten, adc_unit_t eNUnit, stADCHandles_t *stADCHandle);
float adc_read_voltage(stADCHandles_t *stADCHandle);
float read_sensor(stADCHandles_t *stADCHandle, stSensorMap_t *stSensorMap);
float convert_sensor(float fVSensor, stSensorMap_t *stSensorMap);
int adc_read_raw(stADCHandles_t *stADCHandle);
float adc_read_raw_to_percentage(stADCHandles_t *stADCHandle, float min_raw, float max_raw);

#endif // ADC_H