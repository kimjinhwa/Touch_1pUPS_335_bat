#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\URTouch\\hardware\\pic32\\HW_PIC32_defines.h"
// *** Hardwarespecific defines ***
#define cbi(reg, bitmask) (*(reg + 1)) = bitmask
#define sbi(reg, bitmask) (*(reg + 2)) = bitmask
#define rbi(reg, bitmask) (*(reg) & bitmask)

#define pulse_high(reg, bitmask) sbi(reg, bitmask); cbi(reg, bitmask);
#define pulse_low(reg, bitmask) cbi(reg, bitmask); sbi(reg, bitmask);

#define swap(type, i, j) {type t = i; i = j; j = t;}

#define regtype volatile uint32_t
#define regsize uint16_t
