#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT\\tft_drivers\\ili9341\\s4p\\setxy.h"
case ILI9341_S4P:
	LCD_Write_COM(0x2A); //column
	LCD_Write_DATA(x1>>8);
	LCD_Write_DATA(x1);
	LCD_Write_DATA(x2>>8);
	LCD_Write_DATA(x2);
	LCD_Write_COM(0x2B); //page
	LCD_Write_DATA(y1>>8);
	LCD_Write_DATA(y1);
	LCD_Write_DATA(y2>>8);
	LCD_Write_DATA(y2);
	LCD_Write_COM(0x2C); //write
	break;
