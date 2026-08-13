#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT\\hardware\\avr\\HW_AVR.h"
void UTFT::_convert_float(char *buf, double num, int width, byte prec)
{
	dtostrf(num, width, prec, buf);
}
