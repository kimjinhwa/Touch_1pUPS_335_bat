#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\SdFat\\examples\\LowLatencyLoggerADXL345\\UserTypes.h"
#ifndef UserTypes_h
#define UserTypes_h
#include "Arduino.h"
#include "SPI.h"
#define USE_SHARED_SPI 1
#define FILE_BASE_NAME "ADXL4G"
// User data types.  Modify for your data items.
const uint8_t ACCEL_DIM = 3;
struct data_t {
  uint32_t time;
  int16_t accel[ACCEL_DIM];
};
void acquireData(data_t* data);
void printData(Print* pr, data_t* data);
void printHeader(Print* pr);
void userSetup();
#endif  // UserTypes_h
