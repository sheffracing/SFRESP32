#ifndef LTC6811_H
#define LTC6811_H

#include "pin.h"
#include "sfrtypes.h"
#include "driver/spi_master.h"

#include "string.h"
#include "esp_err.h"
#include "esp_log.h"

/* Refer to Device Datasheet for more information on the LTC6811-1 */:
https://www.analog.com/media/en/technical-documentation/data-sheets/LTC6811-1-6811-2.pdf
Command codes page 61
*/

/* --------------------------- Function prototypes -------------------- */
esp_err_t LTC6811_init(uint8_t NCSPin, spi_device_handle_t stDeviceHandle);
esp_err_t BC_Write(spi_device_handle_t stDeviceHandle, uint16_t CMD, uint16_t NBytes, uint8_t *aData);
esp_err_t BC_Read(spi_device_handle_t stDeviceHandle, uint16_t CMD, uint16_t NBytes, uint8_t *aData);


/* --------------------------- Custom Codes -------------------------- */
#define CFGR0  0b00000100 // Configuration Register Group 0 Defaults (REFON = 1)

/* --------------------------- Command Codes -------------------------- */
#define WRCFGA  0b00000000001 // Write Configuration Register Group A
#define WRCFGB  0b00000100100 // Write Configuration Register Group B (unused)
#define RDCFGA  0b00000000010 // Read Configuration Register Group A
#define RDCFGB  0b00000100110 // Read Configuration Register Group B (unused)

#define CLRCELL 0b11100010001 // Clear Cell Voltage Register Groups
#define RDCVA   0b00000000100 // Read Cell Register Group A
#define RDCVB   0b00000000110 // Read Cell Register Group B
#define RDCVC   0b00000001000 // Read Cell Register Group C
#define RDCVD   0b00000001010 // Read Cell Register Group D
#define RDCVE   0b00000001001 // Read Cell Register Group E (unused)
#define RDCVF   0b00000001011 // Read Cell Register Group F (unused)

#define CLRAUX  0b11100010010 // Clear Auxiliary Register Groups
#define RDAXA   0b00000001100 // Read Auxiliary Register Group A
#define RDAXB   0b00000001110 // Read Auxiliary Register Group B
#define RDAXC   0b00000001101 // Read Auxiliary Register Group C (unused)
#define RDAXD   0b00000001111 // Read Auxiliary Register Group D (unused)

#define CLRSTAT 0b11100010011 // Clear Status Register Groups
#define RDSTATA 0b00000010000 // Read Status Register Group A
#define RDSTATB 0b00000010010 // Read Status Register Group B

#define WRSCTRL 0b00000010100 // Write S Control Register Group
#define RDSCTRL 0b00000010110 // Read S Control Register Group

#define WRPWM   0b00000100000 // Write PWM Register Group
#define WRPSB   0b00000011100 // Write PWM/S Control Register Group B (unused)
#define RDPWM   0b00000100010 // Read PWM Register Group 
#define RDPSB   0b00000011110 // Read PWM/S Control Register Group B (unused)

#define STSCTRL 0b00000011001 // Start S Control Pulsing and Poll Status
#define CLRSCTRL 0b00000011000// Clear S Control Register Group

#define PLADC   0b11100010100 // Poll ADC Conversion Status
#define DIAGN   0b11100010101 // Diagnose MUX and Poll Status

#define WRCOMM  0b11100100001 // Write COMM Register Group
#define RDCOMM  0b11100100010 // Read COMM Register Group
#define STCOMM  0b11100100011 // Start I2C/SPI Communication


/* Following commands contain variable values */
// ADCV     0 1 MD[1] MD[0]     1     1 DCP 0   CH[2]   CH[1]    CH[0]  Start Cell Voltage ADC Conversion and Poll Status
// ADOW     0 1 MD[1] MD[0]   PUP     1 DCP 1   CH[2]   CH[1]    CH[0]  Start Open Wire ADC Conversion and Poll Status
// CVST     0 1 MD[1] MD[0] ST[1] ST[0]   0 0       1       1        1  Start Self Test Cell Voltage Conversion and Poll Status
// ADOL     0 1 MD[1] MD[0]     0     0 DCP 0       0       0        1  Start Overlap Measurement of Cell 7 Voltage
// ADAX     1 0 MD[1] MD[0]     1     1   0 0  CHG[2]  CHG[1]  CHG [0]  Start GPIOs ADC Conversion and Poll Status
// ADAXD    1 0 MD[1] MD[0]     0     0   0 0  CHG[2]  CHG[1]  CHG [0]  Start GPIOs ADC Conversion With Digital Redundancy and Poll Status
// AXST     1 0 MD[1] MD[0] ST[1] ST[0]   0 0       1       1        1  Start Self Test GPIOs Conversion and Poll Status
// ADSTAT   1 0 MD[1] MD[0]     1     1   0 1 CHST[2] CHST[1] CHST [0]  Start Status Group ADC Conversion and Poll Status
// ADSTATD  1 0 MD[1] MD[0]     0     0   0 1 CHST[2] CHST[1] CHST [0]  Start Status Group ADC Conversion With Digital Redundancy and Poll Status
// STATST   1 0 MD[1] MD[0] ST[1] ST[0]   0 1       1       1        1  Start Self Test Status Group Conversion and Poll Status
// ADCVAX   1 0 MD[1] MD[0]     1     1 DCP 1       1       1        1  Start Combined Cell Voltage and GPIO1, GPIO2 Conversion and Poll Status
// ADCVSC   1 0 MD[1] MD[0]     1     1 DCP 0       1       1        1  Start Combined Cell Voltage and SC Conversion and Poll Status

/* Commands contain 0s in place of variable values. They should be ORed with desired values */
#define ADCV    0b01001100000
#define ADOW    0b01000101000 
#define CVST    0b01000000111
#define ADOL    0b01000000001
#define ADAX    0b10001100000
#define ADAXD   0b10000000000
#define AXST    0b10000000111
#define ADSTAT  0b10001101000
#define ADSTATD 0b10000001000
#define STATST  0b10000001111
#define ADCVAX  0b10001101111
#define ADCVSC  0b10001100111

/* Command Bit Definitions */
// ADC Mode (MD)        ADCOPT(CFGR0[0]) = 0    ADCOPT(CFGR0[0]) = 1
#define MD_SLOW = 0b00 //  422Hz Mode              1kHz Mode
#define MD_FAST = 0b01 //  27kHz Mode (Fast)       14kHz Mode
#define MD_NORM = 0b10 //  7kHz Mode (Normal)      3kHz Mode
#define MD_FILT = 0b11 //  26Hz Mode (Filtered)    2kHz Mode

// Discharge Permitted (DCP)
#define DCP_OFF = 0b0 // Discharge Not Permitted
#define DCP_ON  = 0b1 // Discharge Permitted

// Cell Channel Selection (CH)
#define CH_ALL  = 0b000 // All Cells
#define CH_1_7  = 0b001 // Cell 1 and Cell 7
#define CH_2_8  = 0b010 // Cell 2 and Cell 8
#define CH_3_9  = 0b011 // Cell 3 and Cell 9
#define CH_4_10 = 0b100 // Cell 4 and Cell 10
#define CH_5_11 = 0b101 // Cell 5 and Cell 11
#define CH_6_12 = 0b110 // Cell 6 and Cell 12

// Pull Up/Pull Down for Open Wire (PUP)
#define PUP_DN = 0b0 // Pull Down
#define PUP_UP = 0b1 // Pull Up

// Self Test Selection (ST)
#define ST_0 = 0b0 // Self Test 1
#define ST_1 = 0b1 // Self Test 2

// GPIO Channel Selection (CHG)
#define CHG_ALL = 0b000 // All GPIOs + 2nd Reference
#define CHG_1   = 0b001 // GPIO 1
#define CHG_2   = 0b010 // GPIO 2
#define CHG_3   = 0b011 // GPIO 3
#define CHG_4   = 0b100 // GPIO 4
#define CHG_5   = 0b101 // GPIO 5
#define CHG_REF = 0b110 // 2nd Reference

// Status Group Selection (CHST)
#define CHST_ALL  = 0b000 // All Status Registers
#define CHST_SC   = 0b001 // Status Register SC
#define CHST_ITMP = 0b010 // Status Register ITMP
#define CHST_VA   = 0b011 // Status Register VA
#define CHST_VD   = 0b100 // Status Register VD

#endif