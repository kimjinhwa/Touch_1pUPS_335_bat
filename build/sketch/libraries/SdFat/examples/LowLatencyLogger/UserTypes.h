#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\SdFat\\examples\\LowLatencyLogger\\UserTypes.h"
#ifndef UserTypes_h
#define UserTypes_h
#include "Arduino.h"
// User data types.  Modify for your data items.
#define FILE_BASE_NAME "adc4pin"
const uint8_t ADC_DIM = 4;
struct data_t {
  uint32_t time;
  uint16_t adc[ADC_DIM];
};
void acquireData(data_t* data);
void printData(Print* pr, data_t* data);
void printHeader(Print* pr);
void userSetup();
#endif  // UserTypes_h
