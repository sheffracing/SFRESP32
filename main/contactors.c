/*Function to operate contactors and check contactor states*/
#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "sfrtypes.h"
#include "driver/ledc.h"


#define CONTACTOR_ON_VOLTAGE_THRESHOLD 5.0  // Voltage threshold to consider contactor as ON
#define DUTYCYCLE_CONTACOTR_ON 100  // Duty cycle to turn on contactor
#define DUTYCYCLE_CONTACOTR_HOLD 80 // Duty cycle to hold contactor on
#define DUTYCYCLE_CONTACOTR_OFF 0  // Duty cycle to turn off contactor
#define CONTACTOR_PWM_FREQUENCY 10000 // PWM frequency in Hz
#define CONTACTOR_PWM_CHANNEL 0 // PWM channel for contactor control

void turn_on_contactor(byte byNContactorID, byte byNContactorGPIOPin) {
    //PWM GPIO to contactor on dutycycle
    ledc_set_duty(LEDC_SPEED_MODE_MAX, CONTACTOR_PWM_CHANNEL, DUTYCYCLE_CONTACOTR_ON); // Set duty cycle to 100
    ledc_update_duty(LEDC_SPEED_MODE_MAX, CONTACTOR_PWM_CHANNEL); // Update duty cycle

    ESP_LOGI("Contactor", "Contactor %d turned ON\n", byNContactorID);
}

void hold_contactor_on(byte byNContactorID, byte byNContactorGPIOPin) {
    //PWM GPIO to contactor hold dutycycle
    ledc_set_duty(LEDC_SPEED_MODE_MAX, CONTACTOR_PWM_CHANNEL, DUTYCYCLE_CONTACOTR_HOLD); // Set duty cycle to 80
    ledc_update_duty(LEDC_SPEED_MODE_MAX, CONTACTOR_PWM_CHANNEL); // Update duty cycle

    ESP_LOGI("Contactor", "Contactor %d held ON\n", byNContactorID);
}


void turn_off_contactor(byte byNContactorID, byte byNContactorGPIOPin) {
    //PWM GPIO to contactor off dutycycle
    ledc_set_duty(LEDC_SPEED_MODE_MAX, CONTACTOR_PWM_CHANNEL, DUTYCYCLE_CONTACOTR_OFF); // Set duty cycle to 0
    ledc_update_duty(LEDC_SPEED_MODE_MAX, CONTACTOR_PWM_CHANNEL); // Update duty cycle

   ESP_LOGI("Contactor", "Contactor %d turned OFF\n", byNContactorID);
}

// Function to check contactor state based on output voltage from relay closed terminal
int check_contactor_state(byte byNContactorID, float fVContactorOut) {
    // Code to check the state of the contactor based on voltage
    if (fVContactorOut > CONTACTOR_ON_VOLTAGE_THRESHOLD) {  // Assuming 5.0V as threshold for ON state
        ESP_LOGI("Contactor", "Contactor %d is closed(Voltage: %.2fV)\n", byNContactorID, fVContactorOut);
        return TRUE;  // ON
    } else {
        ESP_LOGI("Contactor", "Contactor %d is open (Voltage: %.2fV)\n", byNContactorID, fVContactorOut);
        return FALSE;  // OFF
    }
}