#define PEC_INITIAL 16 //Chcek PEC calculation in general
#define PEC_POLYNOMIAL 0x4599

#include "esp_err.h"
#include "esp_log.h"

uint16_t PEC15_calc(uint16_t NBytes, const uint8_t *aData) 
{
    /*
    *===========================================================================
    *   PEC15_calc
    *   Takes:   NLen - Length of data to calculate PEC for
    *            pData - Pointer to data to calculate PEC for
    * 
    *   Returns: 15-bit PEC value
    * 
    *   Calculates the 15-bit Packet Error Code (PEC) for the given data.
    *===========================================================================
    *   Revision History:
    *   28/10/26 GPT5.6 Initial Version
    *
    *===========================================================================
    */
   uint16_t NRemainder = PEC_INITIAL;

    for (uint16_t NByte = 0; NByte < NBytes; NByte++)
    {
        NRemainder ^= (uint16_t)aData[NByte] << 7;
        for (uint8_t NBit = 0; NBit < 8; NBit++)
        {
            if (NRemainder & 0x4000)
            {
                NRemainder = (NRemainder << 1) ^ PEC_POLYNOMIAL;
            }
            else
            {
                NRemainder <<= 1;
            }
        }
    }

    return (NRemainder & 0x7FFF) << 1;
}