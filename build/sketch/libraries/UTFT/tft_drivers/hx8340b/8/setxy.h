#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT\\tft_drivers\\hx8340b\\8\\setxy.h"
case HX8340B_8:
	LCD_Write_COM_DATA(0x02,0x0000); 
	LCD_Write_COM_DATA(0x03,x1); 
	LCD_Write_COM_DATA(0x04,0x0000);
	LCD_Write_COM_DATA(0x05,x2);
	LCD_Write_COM_DATA(0x06,0x0000);
	LCD_Write_COM_DATA(0x07,y1);
	LCD_Write_COM_DATA(0x08,0x0000);
	LCD_Write_COM_DATA(0x09,y2); 
	LCD_Write_COM(0x22);
	break;
