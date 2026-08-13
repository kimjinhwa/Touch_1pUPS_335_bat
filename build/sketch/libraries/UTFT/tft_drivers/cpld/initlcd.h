#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT\\tft_drivers\\cpld\\initlcd.h"
case CPLD:
	if (orient==LANDSCAPE)
		orient = PORTRAIT;
	else
		orient = LANDSCAPE;

	LCD_Write_COM(0x0F);   
	LCD_Write_COM_DATA(0x01,0x0010);
	LCD_Write_COM(0x0F);   
	break;
