#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT\\hardware\\pic32\\HW_PIC32.h"
void UTFT::_convert_float(char *buf, double num, int width, byte prec)
{
	char format[10];
	
	sprintf(format, "%%%i.%if", width, prec);
	sprintf(buf, format, num);
}
