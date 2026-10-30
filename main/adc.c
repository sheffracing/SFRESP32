/*
adc.c
File contains the ADC initialization and calibration functions.
Written by Cole Perera for Sheffield Formula Racing 2025
*/

#include "adc.h"

/* --------------------------- Local Variables ------------------------- */
static adc_oneshot_unit_handle_t sADCUnit1 = NULL;
static adc_oneshot_unit_handle_t sADCUnit2 = NULL;

/* --------------------------- Global Variables ------------------------ */
stADCHandles_t stADCHandle0 =
{
    .eNChannel = APPS1_IN, 
    .stCalibration = NULL,
};
stADCHandles_t stADCHandle1 =
{
    .eNChannel = APPS2_IN, 
    .stCalibration = NULL,
}; 
//Add as needed for more ADC channels

/* --------------------------- Definitions ----------------------------- */


/* --------------------------- Functions ------------------------------- */
esp_err_t adc_register(adc_atten_t eNAtten, adc_unit_t eNUnit, stADCHandles_t *stADCHandle)
{
    /*
    *===========================================================================
    *   adc_register
    *   Takes: eNAtten: ADC attenuation setting
    *          eNUnit: ADC unit to use
    *          stADCHandle0: Pointer to ADC handle structure, contains unit and calibration handles
    * 
    *   Returns: ESP_OK if successful, error code if not.
    * 
    *   Creates a oneshot ADC handle and calibration handle for the specified pin and settings.
    *   The ESP32-C6 has two ADC units, each with multiple channels. Each unit can handle 10
    *   channels refer to the manual for more details. Chapter 39 
    *   https://documentation.espressif.com/esp32-c6_technical_reference_manual_en.pdf
    *=========================================================================== 
    *   Revision History:
    *   31/10/25 CP Initial Version
    *   14/11/25 CP Make work
    *   15/11/25 CP Remove pin parameter, use channel from handle
    *   22/02/26 CP Update to only use one oneshot unit per ADC unit.
    *
    *===========================================================================
    */

    esp_err_t eStatus = ESP_OK;
    adc_oneshot_unit_handle_t stUnitHandle = NULL;

    adc_oneshot_unit_init_cfg_t stADCConfig = {
        .unit_id = eNUnit,
    };

    /* Only create one oneshot unit per ADC unit; reuse it for all channels */
    switch (eNUnit)
    {
        case ADC_UNIT_1:
            if (sADCUnit1 == NULL)
            {
                eStatus = adc_oneshot_new_unit(&stADCConfig, &sADCUnit1);
                if (eStatus != ESP_OK)
                {
                    ESP_LOGE("ADC", "Failed to create ADC1 unit handle: %s", esp_err_to_name(eStatus));
                    return eStatus;
                }
            }
            stUnitHandle = sADCUnit1;
            break;

        case ADC_UNIT_2:
            if (sADCUnit2 == NULL)
            {
                eStatus = adc_oneshot_new_unit(&stADCConfig, &sADCUnit2);
                if (eStatus != ESP_OK)
                {
                    ESP_LOGE("ADC", "Failed to create ADC2 unit handle: %s", esp_err_to_name(eStatus));
                    return eStatus;
                }
            }
            stUnitHandle = sADCUnit2;
            break;

        default:
            ESP_LOGE("ADC", "Unsupported ADC unit: %d", (int)eNUnit);
            return ESP_ERR_INVALID_ARG;
    }

    adc_oneshot_chan_cfg_t stChannelConfig = {
        .atten = eNAtten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    eStatus = adc_oneshot_config_channel(stUnitHandle, stADCHandle->eNChannel, &stChannelConfig);
    if (eStatus != ESP_OK) {
        ESP_LOGE("ADC", "Failed to configure ADC channel: %s", esp_err_to_name(eStatus));
        return eStatus;
    }

    /* Store the unit handle in the channel handle for later reads */
    stADCHandle->stADCUnit = stUnitHandle;

    adc_cali_curve_fitting_config_t stCalibrationConfig = {
        .unit_id = eNUnit,
        .chan = stADCHandle->eNChannel,
        .atten = eNAtten,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    
    eStatus = adc_cali_create_scheme_curve_fitting(&stCalibrationConfig, &stADCHandle->stCalibration);
    if (eStatus != ESP_OK) {
        ESP_LOGE("ADC", "Failed to create calibration handle: %s", esp_err_to_name(eStatus));
    }

    return eStatus;
}

int adc_read_raw(stADCHandles_t *stADCHandle)
/*
*===========================================================================
*   adc_read_raw
*   Takes:  stADCHandle: Pointer to ADC handle structure, contains unit and calibration handles
* 
*   Returns: raw ADC reading as integer
* 
*   Uses the ADC handle to read the level from the specified ADC channel and
*   returns the level as a 10 bit integer.
*=========================================================================== 
*   Revision History:
*   13/10/26 DH Initial Version
*
*===========================================================================
*/
{
    int NVADCRaw = 0;
    adc_oneshot_read(stADCHandle->stADCUnit, stADCHandle->eNChannel, &NVADCRaw);
    return NVADCRaw; // Return raw ADC reading
}

float adc_read_raw_to_percentage(stADCHandles_t *stADCHandle, float min_raw, float max_raw) 
/*
*===========================================================================
*   adc_read_raw_to_percentage
*   Takes:  stADCHandle: Pointer to ADC handle structure, contains unit and calibration handles
*           min_raw: Minimum raw ADC value for the sensor
*           max_raw: Maximum raw ADC value for the sensor
* 
*   Returns: Normalised sensor reading as float
* 
*   Converts the raw ADC reading to a percentage based on the sensor's range.
*=========================================================================== 
*   Revision History:
*   14/10/26 DH Initial Version
*
*===========================================================================
*/
{
    int NVADCRaw = 0;
    adc_oneshot_read(stADCHandle->stADCUnit, stADCHandle->eNChannel, &NVADCRaw);
    ESP_LOGI("ADC", "ADC read: %d", NVADCRaw);
    float actual_min = (min_raw < max_raw) ? min_raw : max_raw;
    float actual_max = (min_raw > max_raw) ? min_raw : max_raw;
    //ESP_LOGI(SFR_TAG, "Sensor %f raw value: %f ",stADCHandle->stADCUnit, NVADCRaw);
    if ((float)NVADCRaw < (actual_min - 5.0f) || (float)NVADCRaw > (actual_max + 5.0f)) {
        return -999.0f; // Invalid reading
    }

    // Linear interpolation formula: ((raw - min) / (max - min)) * 100.0
    float percentage = ((float)NVADCRaw - min_raw) / (max_raw - min_raw) * 100.0f;
    
    // Clamp result to valid 0% - 100% range to handle sensor noise
    if (percentage < 0.0f) {
        return 0.0f;
    } else if (percentage > 100.0f) {
        return 100.0f;
    }
    return percentage;
}

float adc_read_voltage(stADCHandles_t *stADCHandle)
/*
*===========================================================================
*   adc_read_voltage
*   Takes:  stADCHandle: Pointer to ADC handle structure, contains unit and calibration handles
* 
*   Returns: voltage in volts as float
* 
*   Uses the ADC handle to read the voltage from the specified ADC channel and
*   returns the voltage in volts as a float. Uses the calibration for the 
*   conversion from raw adc counts to volts.
*=========================================================================== 
*   Revision History:
*   31/10/25 CP Initial Version
*
*===========================================================================
*/
{
    int NVADCRaw = 0;
    int NVADC = 0;
    adc_oneshot_read(stADCHandle->stADCUnit, stADCHandle->eNChannel, &NVADCRaw);
    adc_cali_raw_to_voltage(stADCHandle->stCalibration, NVADCRaw, &NVADC);
    return (float)NVADC / 1000.0f; // Convert mV to V
}

float read_sensor(stADCHandles_t *stADCHandle, stSensorMap_t *stSensorMap)
/*
*===========================================================================
*   read_sensor
*   Takes:  stADCHandle: Pointer to ADC handle structure, contains unit and calibration handles
*           stSensorMap: Pointer to sensor map structure for lookup table and limits
* 
*   Returns: Normalised sensor reading as float, or -999.0f on error
* 
*   Reads from the ADC and uses the sensor map to convert this to a real value.
*   Includes plausibility check based on sensor map limits for SCS compliance.
*
*=========================================================================== 
*   Revision History:
*   16/11/25 CP Initial Version
*
*===========================================================================
*/
{
    uint8_t NCounter;
    float fVSensor = adc_read_voltage(stADCHandle);
    /* If outside plauseable range throw error (SCS Requirement) */
    if (fVSensor < stSensorMap->fLowerLimit || fVSensor > stSensorMap->fUpperLimit)
    {
        return -999.0f;
    }

    /* Lookup Sensor Value */
    for (NCounter = 0; NCounter < sizeof(stSensorMap->afLookupTable[0])/sizeof(stSensorMap->afLookupTable[0][0]) - 1; NCounter++)
    {
        if (fVSensor >= stSensorMap->afLookupTable[0][NCounter] && fVSensor < stSensorMap->afLookupTable[0][NCounter + 1])
        { 
            float fSlope = (stSensorMap->afLookupTable[1][NCounter + 1] - stSensorMap->afLookupTable[1][NCounter]) /
                           (stSensorMap->afLookupTable[0][NCounter + 1] - stSensorMap->afLookupTable[0][NCounter]);
            float fOutput = stSensorMap->afLookupTable[1][NCounter] +
                            fSlope * (fVSensor - stSensorMap->afLookupTable[0][NCounter]);
            return fOutput;
        }
    }
    
    return -999.0f;
}

float convert_sensor(float fVSensor, stSensorMap_t *stSensorMap)
/*
*===========================================================================
*   convert_sensor
*   Takes:  fVoltage: The voltage reading from the ADC
*           stSensorMap: Pointer to sensor map structure for lookup table and limits
* 
*   Returns: Normalised sensor reading as float, or -999.0f on error
* 
*   Uses the sensor map to convert the voltage to a real value.
*   Includes plausibility check based on sensor map limits for SCS compliance.
*
*=========================================================================== 
*   Revision History:
*   16/11/25 CP Initial Version
*
*===========================================================================
*/
{
    uint8_t NCounter;
    /* If outside plauseable range throw error (SCS Requirement) */
    if (fVSensor < stSensorMap->fLowerLimit || fVSensor > stSensorMap->fUpperLimit)
    {
        return -999.0f;
    }

    /* Lookup Sensor Value */
    for (NCounter = 0; NCounter < sizeof(stSensorMap->afLookupTable[0])/sizeof(stSensorMap->afLookupTable[0][0]) - 1; NCounter++)
    {
        if (fVSensor >= stSensorMap->afLookupTable[0][NCounter] && fVSensor < stSensorMap->afLookupTable[0][NCounter + 1])
        { 
            float fSlope = (stSensorMap->afLookupTable[1][NCounter + 1] - stSensorMap->afLookupTable[1][NCounter]) /
                           (stSensorMap->afLookupTable[0][NCounter + 1] - stSensorMap->afLookupTable[0][NCounter]);
            float fOutput = stSensorMap->afLookupTable[1][NCounter] +
                            fSlope * (fVSensor - stSensorMap->afLookupTable[0][NCounter]);
            return fOutput;
        }
    }
    
    return -999.0f;
}