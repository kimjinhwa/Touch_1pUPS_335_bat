#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\SdFat\\examples\\LowLatencyLoggerMPU6050\\UserTypes.h"
#ifndef UserTypes_h
#define UserTypes_h
#include "Arduino.h"
#define FILE_BASE_NAME "mpuraw"
struct data_t {
  unsigned long time;
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
};
void acquireData(data_t* data);
void printData(Print* pr, data_t* data);
void printHeader(Print* pr);
void userSetup();
#endif  // UserTypes_h
