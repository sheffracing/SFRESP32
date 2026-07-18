/*
contactors.h
Header file for contactor control functions

Written by Aditya Parnandi for Sheffield Formula Racing 2025
*/

#ifndef SFR_CONTACTORS
#define SFR_CONTACTORS

#include "sfrtypes.h"

/* Function Declarations */
void turn_on_contactor(byte byNContactorID, byte byNContactorGPIOPin);
void hold_contactor_on(byte byNContactorID, byte byNContactorGPIOPin);
void turn_off_contactor(byte byNContactorID, byte byNContactorGPIOPin);
int check_contactor_state(byte byNContactorID, float fVContactorOut);

#endif // SFR_CONTACTORS
