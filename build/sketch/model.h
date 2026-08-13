#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\model.h"

void ac_model(int x_line, int y_line)
{
	myGLCD.setColor(255, 255, 255);
	myGLCD.drawCircle(x_line,y_line,5);
	myGLCD.drawCircle(x_line,y_line,4);
	myGLCD.drawCircle((x_line+10),y_line,5);
	myGLCD.drawCircle((x_line+10),y_line,4);
	myGLCD.setColor(0,0,0);
	myGLCD.fillRect((x_line-5),(y_line+2),(x_line+4),(y_line+5));
	myGLCD.fillRect((x_line+6),(y_line-5),(x_line+15),(y_line-20));
}