// TODO: replace that library with something that uses the SPI hardware units insted of delays.
#include <Arduino.h>
#include "PT6302.h"



#define CLKB 2
#define RSTB 3
#define CSB  4
#define DIN  5


// Configuration for SPI0



PT6302 vfd(CLKB, RSTB, CSB, DIN);


void displaySetup();