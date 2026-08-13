#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\URTouch\\hardware\\avr\\HW_AVR_defines.h"
// *** Hardwarespecific defines ***
#define cbi(reg, bitmask) *reg &= ~bitmask
#define sbi(reg, bitmask) *reg |= bitmask
#define rbi(reg, bitmask) ((*reg) & bitmask)

#define pulse_high(reg, bitmask) sbi(reg, bitmask); cbi(reg, bitmask);
#define pulse_low(reg, bitmask) cbi(reg, bitmask); sbi(reg, bitmask);

#define swap(type, i, j) {type t = i; i = j; j = t;}

#define regtype volatile uint8_t
#define regsize uint8_t
