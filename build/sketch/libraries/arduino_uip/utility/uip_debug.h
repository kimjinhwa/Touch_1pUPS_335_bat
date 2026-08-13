#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\arduino_uip\\utility\\uip_debug.h"
#ifndef UIP_DEBUG_H
#define UIP_DEBUG_H
extern "C" {
  #include "utility/uip.h"
}

class UIPDebug {

public:

  static void uip_debug_printconns();
  static bool uip_debug_printcon(struct uip_conn *lhs,struct uip_conn *rhs);
  static void uip_debug_printbytes(const uint8_t *data, uint8_t len);

};


#endif
