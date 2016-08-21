#ifndef _DEF_THINGS_FUNC_
#define _DEF_THINGS_FUNC_

#include <TM1637Display.h>

// Module connection pins (Digital Pins)
#define CLK D5
#define DIO D6

#define DBG_OUTPUT_PORT Serial
/*
Measuring AC Current Using ACS712
*/
const int sensorIn = A0;
const int switchOut = D2;

void initThings(Thing (*ptrThings)[THINGS_LEN], unsigned long ntp_timer);
void processThings(Thing (*ptrThings)[THINGS_LEN], Recipe (*ptrRecipes)[RECIPES_LEN], long nodeId, unsigned long ntp_timer);
int determineVQ(int PIN);

#endif
