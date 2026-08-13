#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT\\tft_drivers\\cpld\\setxy.h"
case CPLD:
	LCD_Write_COM_DATA(0x02, y1);
	LCD_Write_COM_DATA(0x03, x1);
   	LCD_Write_COM_DATA(0x06, y2);
	LCD_Write_COM_DATA(0x07, x2);
	LCD_Write_COM(0x0F);					 						 
	break;
