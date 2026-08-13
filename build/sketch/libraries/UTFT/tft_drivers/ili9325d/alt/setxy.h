#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT\\tft_drivers\\ili9325d\\alt\\setxy.h"
case ILI9325D_16ALT:
	LCD_Write_COM_DATA(0x20,x1);
	LCD_Write_COM_DATA(0x21,y1);
	LCD_Write_COM_DATA(0x50,x1);
	LCD_Write_COM_DATA(0x52,y1);
	LCD_Write_COM_DATA(0x51,x2);
	LCD_Write_COM_DATA(0x53,y2);
	LCD_Write_COM(0x22); 
	break;
