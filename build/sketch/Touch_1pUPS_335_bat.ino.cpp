#include <Arduino.h>
#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\Touch_1pUPS_335_bat.ino"
#include <SPI.h> // needed for Arduino versions later than 0018
#include <UIPEthernet.h>
#include <UIPUDP.h> // UDP library from: bjoern@cs.stanford.edu 12/30/2008
#include <SdFat.h>
#include <UTFT.h>
#include <URTouch.h>
#include <UTFT_Buttons.h>
#include <UTFT_SdRaw.h>
#include <DueTimer.h>
#include <RTCDue.h>
#include <SPIMemory.h>
#include "watchdog.cpp"
#include <DueFlashStorage.h>
#include <math.h>

#if defined(__AVR__)
#include <avr/pgmspace.h>
#elif defined(__PIC32MX__)
#define PROGMEM
#elif defined(__arm__)
#define PROGMEM
#endif

#define WDT_KEY (0xA5)
#define SD_CHIP_SELECT 78 // SD chip select pin

#define intro_set 0
#define intro_set_i 100
#define intro 16
#define intro_i 116
#define main 1
#define main_next 101
#define main_i 121
#define sett 2
#define sett_i 102
#define meter 3
#define meter_i 103
#define control 4
#define control_i 104
#define control_next 114
#define alarm1 5
#define alarm1_i 105
#define alarm2 6
#define alarm2_i 106
#define alarm3 13
#define alarm3_i 112
#define secret1 7
#define secret1_i 107
#define secret2 8
#define secret2_i 108
#define keypad 9
#define keypad_i 109
#define history 10
#define history_next 11
#define history_i 110
#define timeset 12
#define timeset_i 111
#define time_set 201
#define offset 14
#define offset_i 124
#define black 15
#define black_i 125

#define battery_voltage_ref_keypad 61
#define battery_current_ref_keypad 62
#define output_voltage_ref_keypad 63
#define discharge_set_bat_voltage_keypad 64

#define input_voltage_keypad 71
#define input_current_keypad 72
#define inverter_voltage_keypad 73
#define inverter_current_keypad 74
#define battery_voltage_keypad 75
#define battery_current_keypad 76
#define output_current_keypad 77
#define dc_link_keypad 78

#define year_keypad 81
#define month_keypad 82
#define day_keypad 83
#define hour_keypad 84
#define minute_keypad 85
#define second_keypad 86

void tx_read();
void alarm_tx();
void data_tx();
void tx_write();
void interrupt();
void touch_read();
void eeprom_history();
void eeprom_write();
void eeprom_set();
void serial_rx();
void background_menu();
void background_meter();
void background_gain();
void background_alarm();
void background_history();
void buzz_set();
void alarm_set();
void drawButtons();
void updateStr(int val);
void buffer_empty();
void buffer_full();
void position(int address, int x1_check, int y1_check);
void position2(int address, int x1_check, int y1_check);
void position3(int address, int x1_check, int y1_check);
void segment_position(int address, int x1_check, int y1_check);
void time_segment_position(int address, int x1_check, int y1_check);
void waitForIt(int x1, int y11, int x2, int y2);
void ac_model(int x_line, int y_line);

RTCDue rtc(XTAL);
SdFat sd;
ArduinoOutStream cout(Serial);

DueFlashStorage dueFlashStorage;

File myFile;
// UTFT    myGLCD(TFT01_70,38,39,40,41);	//TFT set
// UTFT    myGLCD(ITDB50,38,39,40,41);	//TFT set
UTFT myGLCD(ILI9327, 38, 39, 40, 41); // TFT set
URTouch myTouch(6, 5, 4, 3, 2);		  // Touch set

SPIFlash flash(87);

UTFT_SdRaw myFiles(&myGLCD);
UTFT_Buttons myButtons(&myGLCD, &myTouch);

/* Change these values to set the current initial time */
const uint8_t seconds = 24;
const uint8_t minutes = 59;
const uint8_t hours = 11;

/* Change these values to set the current initial date */
const uint8_t day = 12;
const uint8_t month = 11;
const uint16_t year = 2018;

const char *daynames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

/**********font**********/
extern uint8_t BigFont[];
extern uint8_t SmallFont[];
extern uint8_t Retro[];
extern uint8_t SevenSegNumFont[];
extern uint8_t SevenSegNumFontMDS[];
extern uint8_t Segment[];
extern uint8_t SevenSegNumFontPlusPlus[];
/************************/
extern unsigned short h_menu_ctrl[736];
extern unsigned short h_menu_me[736];
extern unsigned short h_menu_set[736];
extern unsigned short h_menu_alarm[736];
extern unsigned short speaker[2500];
extern unsigned short enter[6345];
extern unsigned short tps[21700];
extern unsigned short scanjet[24750];

uint8_t intro_type = 0;
int y1_delete;
int input_set_data, input_set_data_l, input_set_data_h, set_address;

int eeprom_address, eeprom_address_h, eeprom_address_l, eeprom_plus;

int buzz_snmp = 0, buzz_snmp_old = 0;

unsigned int dis_lock = 0;
int leesh;
int buzzer = 8, tx_toggle = 0;
boolean buzzer_switch = 1, ac_onoff, touch_check, con_check, language = 0, buzzer_click = 0;
char stCurrent[4] = "";
int stCurrentLen = 0;
// char name_test[26]="Input current limit fault";
int x, y, i, j, keypad_set;
int alarm_count, main_system_check1, main_system_check2;
int page_num = 0;
int serial_length;
int timer300ms, timer50ms, timer1500ms, timer1000ms;
int hex_data[99];
unsigned short eeprom_data;
unsigned char Scic_Tx_Frame_Buf[11] = {
	0,
};
unsigned char rx[109] = {
	0,
};
int crc_tx_data = 0, crc_tx_h = 0, crc_tx_l = 0, crc_rx_data = 0, crc_rx_h = 0, crc_rx_l = 0;

int control_alarm;
int control_check;
int control_check_save;

short alarm_check, main_check;
short ups_hw_check1, ups_hw_check2;
int ups_hw_state1, ups_hw_state2;
int alarm_number_i, alarm_number_1i, alarm_number_2i, k, y11, z;
unsigned int alarm_number, alarm_number1, alarm_number2, alarm_data1, alarm_data2;
int hex_hw2, hex_hw2_h, hex_hw1, hex_hw1_h;

int see_eeprom;

unsigned int rtc_year, rtc_month, rtc_day, rtc_hour, rtc_minute, rtc_second;
unsigned short set_year, set_month, set_day, set_hour, set_minute, set_second;

unsigned char auchCRCHi[] = {																  // Table of CRC values for high. order byte
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, // 0-14
	0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, // 15-29
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, // 30-44
	0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, // 45-59
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, // 60-74
	0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, // 75-89
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, // 90-104
	0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, // 105-119
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, // 120-134
	0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, // 135-149
	0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, // 150-164
	0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, // 165-179
	0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, // 180-194
	0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, // 195-209
	0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, // 210-224
	0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, // 225-239
	0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, // 240-254
	0x40};
unsigned char auchCRCLo[] = {																  // Table of CRC values for low. order byte
	0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4, // 0-14
	0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09, // 15-29
	0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD, // 30-44
	0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3, // 45-59
	0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7, // 60-74
	0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A, // 75-89
	0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE, // 90-104
	0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26, // 105-119
	0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2, // 120-134
	0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F, // 135-149
	0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB, // 150-164
	0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5, // 165-179
	0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91, // 180-194
	0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C, // 195-209
	0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88, // 210-224
	0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C, // 225-239
	0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80, // 240-254
	0x40};

#line 246 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\Touch_1pUPS_335_bat.ino"
int MODBUS_CRC(unsigned char *auchMsg, int usDataLen);
#line 261 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\Touch_1pUPS_335_bat.ino"
void setupWatchDog();
#line 267 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\Touch_1pUPS_335_bat.ino"
void WatchDogClear();
#line 2744 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\Touch_1pUPS_335_bat.ino"
void setup();
#line 2867 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\Touch_1pUPS_335_bat.ino"
void loop();
#line 246 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\Touch_1pUPS_335_bat.ino"
int MODBUS_CRC(unsigned char *auchMsg, int usDataLen)
{
	unsigned char uchCRCHi = 0xFF; // high byte of CRC initialized
	unsigned char uchCRCLo = 0xFF; // low byte of CRC initialized
	unsigned char uIndex;		   // will index into CRC lookup table

	while (usDataLen--)
	{
		uIndex = uchCRCHi ^ *auchMsg++; // calculate
		uchCRCHi = uchCRCLo ^ auchCRCHi[uIndex];
		uchCRCLo = auchCRCLo[uIndex];
	}
	return ((int)uchCRCHi << 8 | (int)uchCRCLo);
}

void setupWatchDog()
{
	uint32_t wdp_ms = 1152;
	WDT_Enable(WDT, 0x2000 | wdp_ms | (wdp_ms << 16));
}

void WatchDogClear()
{
	// Restart watchdo0-.g
	WDT->WDT_CR = WDT_CR_KEY(WDT_KEY) | WDT_CR_WDRSTT;
}

void tx_read()
{
	Scic_Tx_Frame_Buf[0] = 0x55;
	Scic_Tx_Frame_Buf[1] = 0x04;
	Scic_Tx_Frame_Buf[2] = 0x00;
	Scic_Tx_Frame_Buf[4] = 0x00;
	crc_tx_data = MODBUS_CRC(Scic_Tx_Frame_Buf, 6);
	crc_tx_h = (crc_tx_data >> 8) & 0xff;
	crc_tx_l = (crc_tx_data & 0xff);
	Scic_Tx_Frame_Buf[6] = crc_tx_h;
	Scic_Tx_Frame_Buf[7] = crc_tx_l;
	Serial1.write(Scic_Tx_Frame_Buf, 8);
	//		Serial1.flush();
	serial_rx();
}
void alarm_tx()
{
	Scic_Tx_Frame_Buf[3] = 0x0A;
	Scic_Tx_Frame_Buf[5] = 0x03;
	tx_read();
}

void data_tx()
{
	if (page_num == meter_i)
	{
		Scic_Tx_Frame_Buf[3] = 0x1E;
		Scic_Tx_Frame_Buf[5] = 0x0C;
	}
	else if (page_num == main_i)
	{
		Scic_Tx_Frame_Buf[3] = 0x06;
		Scic_Tx_Frame_Buf[5] = 0x02;
	}
	else if (page_num == intro_i)
	{
		Scic_Tx_Frame_Buf[3] = 0x00;
		Scic_Tx_Frame_Buf[5] = 0x04;
	}
	else if (page_num == control_i)
	{
		Scic_Tx_Frame_Buf[3] = 0x0F;
		Scic_Tx_Frame_Buf[5] = 0x06;
	}
	else if (page_num == sett_i)
	{
		Scic_Tx_Frame_Buf[3] = 0x32;
		Scic_Tx_Frame_Buf[5] = 0x08;
	}
	else if ((page_num == secret1_i) || (page_num == secret2_i))
	{
		Scic_Tx_Frame_Buf[3] = 0x3C;
		Scic_Tx_Frame_Buf[5] = 0x13;
	}
	else if (page_num == offset_i)
	{
		Scic_Tx_Frame_Buf[3] = 0x5A;
		Scic_Tx_Frame_Buf[5] = 0x09;
	}
	tx_read();
}
void tx_write()
{
	Scic_Tx_Frame_Buf[0] = 0x55;
	Scic_Tx_Frame_Buf[1] = 0x10;
	Scic_Tx_Frame_Buf[2] = 0x00;
	Scic_Tx_Frame_Buf[3] = set_address;
	Scic_Tx_Frame_Buf[4] = 0x00;
	Scic_Tx_Frame_Buf[5] = 0x01;
	Scic_Tx_Frame_Buf[6] = 0x02;
	input_set_data_h = (input_set_data >> 8) & 0xff;
	input_set_data_l = (input_set_data & 0xff);
	Scic_Tx_Frame_Buf[7] = input_set_data_h;
	Scic_Tx_Frame_Buf[8] = input_set_data_l;
	crc_tx_data = MODBUS_CRC(Scic_Tx_Frame_Buf, 9);
	crc_tx_h = (crc_tx_data >> 8) & 0xff;
	crc_tx_l = (crc_tx_data & 0xff);
	Scic_Tx_Frame_Buf[9] = crc_tx_h;
	Scic_Tx_Frame_Buf[10] = crc_tx_l;

	Serial1.write(Scic_Tx_Frame_Buf, 11);
	//		Serial1.flush();
}
//#define SCREEN_SAVER 600 
void interrupt()
{

	timer300ms++;
	timer50ms++;
	timer1500ms++;
	timer1000ms++;

	if (buzzer_switch == 1)
	{
		if (timer1500ms < 400)
			digitalWrite(buzzer, 1);
		else
			digitalWrite(buzzer, 0);
	}
	else if (buzzer_switch != 1)
		digitalWrite(buzzer, 0);

	if (timer1500ms > 1499)
	{
		timer1500ms = 0;
	}
	if (timer50ms > 49)
	{
		timer50ms = 0;
		touch_check = 1;
	}
	if (timer1000ms > 999)
	{
		timer1000ms = 0;
		dis_lock++;
		#ifdef SCREEN_SAVER
		if (dis_lock > SCREEN_SAVER)
		{
			if (dis_lock == SCREEN_SAVER + 1)
			{
				page_num = black;
			}
			if (dis_lock > 65000)
				dis_lock = SCREEN_SAVER +2;
		}
		#endif
	}
}

void touch_read()
{

	if (myTouch.dataAvailable())
	{
		myTouch.read();
		x = myTouch.getX();
		y = 239 - myTouch.getY();
		dis_lock = 0;
		switch (page_num)
		{
		case black_i:
			if ((x >= 0) && (x <= 319) && (y >= 0) && (y <= 239))
			{
				page_num = main;
			}
			break;
		case intro_set_i:
			if ((x >= 0) && (x <= 159) && (y >= 0) && (y <= 119))
			{
				waitForIt(0, 0, 159, 119);
				dueFlashStorage.write(4098, 1);
				intro_type = 1;
				page_num = intro;
			}
			if ((x >= 160) && (x <= 319) && (y >= 0) && (y <= 119))
			{
				waitForIt(160, 0, 319, 119);
				dueFlashStorage.write(4098, 2);
				intro_type = 2;
				page_num = intro;
			}
			if ((x >= 0) && (x <= 159) && (y >= 120) && (y <= 239))
			{
				waitForIt(0, 120, 159, 239);
				dueFlashStorage.write(4098, 3);
				intro_type = 3;
				page_num = intro;
			}
			if ((x >= 160) && (x <= 319) && (y >= 120) && (y <= 239))
			{
				waitForIt(160, 120, 319, 239);
				dueFlashStorage.write(4098, 4);
				intro_type = 4;
				page_num = intro;
			}
			break;
		case intro_i:
			if ((x >= 0) && (x <= 30) && (y >= 210) && (y <= 239))
			{
				waitForIt(0, 210, 30, 239);
				intro_type = 0;
				page_num = intro_set;
			}
			if ((x >= 89) && (x <= 230) && (y >= 185) && (y <= 228))
			{
				waitForIt(89, 185, 230, 228);
				page_num = main;
			}
			if ((intro_type == 1) || (intro_type == 3))
			{
				if ((x >= 220) && (x <= 269) && (y >= 0) && (y <= 24))
				{
					waitForIt(220, 0, 269, 24);
					dueFlashStorage.write(4097, 0);
					language = 0;
					page_num = intro;
				}
				if ((x >= 270) && (x <= 319) && (y >= 0) && (y <= 24))
				{
					waitForIt(270, 0, 319, 24);
					dueFlashStorage.write(4097, 1);
					language = 1;
					page_num = intro;
				}
			}
			//      			}
			break;
		case main_i:
			if ((y >= 1) && (y <= 30) && (x >= 1) && (x <= 100))
			{
				waitForIt(1, 1, 100, 30);
				page_num = intro;
			}
			if ((y >= 200) && (y <= 238) && (x >= 1) && (x <= 79))
			{
				waitForIt(1, 200, 79, 238);
				page_num = control;
			}
			if ((y >= 200) && (y <= 238) && (x >= 81) && (x <= 159))
			{
				waitForIt(81, 200, 159, 238);
				page_num = meter;
			}
			if ((y >= 200) && (y <= 238) && (x >= 161) && (x <= 239))
			{
				waitForIt(161, 200, 239, 238);
				page_num = sett;
			}
			if ((y >= 200) && (y <= 238) && (x >= 241) && (x <= 318))
			{
				waitForIt(241, 200, 318, 238);
				page_num = alarm1;
			}
			if (buzzer_switch == 1)
			{
				if ((y >= 135) && (y <= 184) && (x >= 25) && (x <= 84))
				{
					waitForIt(25, 135, 74, 184);
					buzzer_switch = 0;
					digitalWrite(buzzer, 0);
					buzzer_click = 0;
					buzz_snmp_old = buzz_snmp; // snmp toggle
				}
			}
			//    	  		}
			break;

		case sett_i:
			if ((x >= 220) && (x <= 302) && (y >= 128) && (y <= 157))
			{
				waitForIt(220, 128, 302, 157);
				keypad_set = discharge_set_bat_voltage_keypad;
				page_num = keypad;
			}
			if ((x >= 220) && (x <= 302) && (y >= 88) && (y <= 117))
			{
				waitForIt(220, 88, 302, 117);
				keypad_set = output_voltage_ref_keypad;
				page_num = keypad;
			}
			if ((x >= 220) && (x <= 302) && (y >= 8) && (y <= 37))
			{
				waitForIt(220, 8, 302, 37);
				keypad_set = battery_voltage_ref_keypad;
				page_num = keypad;
			}
			if ((x >= 220) && (x <= 302) && (y >= 48) && (y <= 77))
			{
				waitForIt(220, 48, 302, 77);
				keypad_set = battery_current_ref_keypad;
				page_num = keypad;
			}
			if ((x >= 0) && (x <= 20) && (y >= 220) && (y <= 239))
			{
				waitForIt(0, 220, 20, 239);
				page_num = secret1;
			}
			if ((x >= 80) && (x <= 190) && (y >= 185) && (y <= 235))
			{
				waitForIt(80, 185, 190, 235);
				page_num = timeset;
			}
			if ((x >= 200) && (x <= 310) && (y >= 185) && (y <= 235))
			{
				waitForIt(200, 185, 310, 235);
				page_num = main;
			}
			//    	  		}
			break;
		case meter_i:
			if ((x >= 200) && (x <= 310) && (y >= 185) && (y <= 235))
			{
				waitForIt(200, 185, 310, 235);
				page_num = main;
			}
			//     	  		}
			break;
		case control_i:
			if ((x >= 224) && (x <= 312) && (y >= 7) && (y <= 50))
			{
				waitForIt(224, 7, 312, 50);
				input_set_data = hex_data[15] ^ 0x0001;
				set_address = 0x0F;
				tx_write();
			}
			if ((x >= 224) && (x <= 312) && (y >= 59) && (y <= 102))
			{
				waitForIt(224, 59, 312, 102);
				input_set_data = hex_data[15] ^ 0x0008;
				set_address = 0x0F;
				tx_write();
			}

			if ((x >= 200) && (x <= 310) && (y >= 185) && (y <= 235))
			{
				waitForIt(200, 185, 310, 235);
				page_num = main;
			}
			//      			}
			break;
		case alarm1_i:
			if ((y >= 200) && (y <= 238) && (x >= 1) && (x <= 79))
			{
				waitForIt(1, 200, 79, 238);
				page_num = history;
			}
			if ((y >= 200) && (y <= 238) && (x >= 161) && (x <= 239))
			{
				Serial.println(alarm_number_i);
				if (alarm_number_i > 7)
				{
					waitForIt(161, 200, 239, 238);
					page_num = alarm2;
				}
			}
			if ((y >= 200) && (y <= 238) && (x >= 241) && (x <= 318))
			{
				waitForIt(241, 200, 318, 238);
				page_num = main;
			}
			//     	  		}
			break;
		case alarm2_i:
			if ((y >= 200) && (y <= 238) && (x >= 1) && (x <= 79))
			{
				waitForIt(1, 200, 79, 238);
				page_num = history;
			}
			if ((y >= 200) && (y <= 238) && (x >= 81) && (x <= 159))
			{
				waitForIt(81, 200, 159, 238);
				page_num = alarm1;
			}
			if ((y >= 200) && (y <= 238) && (x >= 161) && (x <= 239))
			{
				if (alarm_number_i > 14)
				{
					waitForIt(161, 200, 239, 238);
					page_num = alarm3;
				}
			}
			if ((y >= 200) && (y <= 238) && (x >= 241) && (x <= 318))
			{
				waitForIt(241, 200, 318, 238);
				page_num = main;
			}
			//     	 		  }
			break;
		case alarm3_i:
			if ((y >= 200) && (y <= 238) && (x >= 1) && (x <= 79))
			{
				waitForIt(1, 200, 79, 238);
				page_num = history;
			}
			if ((y >= 200) && (y <= 238) && (x >= 81) && (x <= 159))
			{
				waitForIt(81, 200, 159, 238);
				page_num = alarm2;
			}
			if ((y >= 200) && (y <= 238) && (x >= 241) && (x <= 318))
			{
				waitForIt(241, 200, 318, 238);
				page_num = main;
			}
			//    	  		}
			break;
		case secret1_i:
			myGLCD.setColor(0, 0, 0);
			if ((x >= 20) && (x <= 150) && (y >= 30) && (y <= 87))
			{
				waitForIt(20, 30, 150, 87);
				keypad_set = input_voltage_keypad;
				page_num = keypad;
			}
			if ((x >= 170) && (x <= 300) && (y >= 30) && (y <= 87))
			{
				waitForIt(170, 30, 300, 87);
				keypad_set = input_current_keypad;
				page_num = keypad;
			}
			if ((x >= 20) && (x <= 150) && (y >= 126) && (y <= 183))
			{
				waitForIt(20, 126, 150, 183);
				keypad_set = inverter_voltage_keypad;
				page_num = keypad;
			}
			if ((x >= 170) && (x <= 300) && (y >= 126) && (y <= 183))
			{
				waitForIt(170, 126, 300, 183);
				keypad_set = inverter_current_keypad;
				page_num = keypad;
			}
			if ((y >= 200) && (y <= 238) && (x >= 1) && (x <= 79))
			{
				waitForIt(1, 200, 79, 238);
				page_num = main;
			}
			if ((y >= 200) && (y <= 238) && (x >= 81) && (x <= 159))
			{
				waitForIt(81, 200, 159, 238);
				page_num = secret2;
			}
			if ((y >= 200) && (y <= 238) && (x >= 161) && (x <= 239))
			{
				waitForIt(161, 200, 239, 238);
				page_num = offset;
			}
			if ((y >= 200) && (y <= 238) && (x >= 241) && (x <= 318))
			{
				waitForIt(241, 200, 318, 238);
				page_num = sett;
			}
			//    	  		}
			break;
		case secret2_i:
			if ((x >= 20) && (x <= 150) && (y >= 30) && (y <= 87))
			{
				waitForIt(20, 30, 150, 87);
				keypad_set = battery_voltage_keypad;
				page_num = keypad;
			}
			if ((x >= 170) && (x <= 300) && (y >= 30) && (y <= 87))
			{
				waitForIt(170, 30, 300, 87);
				keypad_set = battery_current_keypad;
				page_num = keypad;
			}
			if ((x >= 20) && (x <= 150) && (y >= 126) && (y <= 183))
			{
				waitForIt(20, 126, 150, 183);
				keypad_set = output_current_keypad;
				page_num = keypad;
			}
			if ((x >= 170) && (x <= 300) && (y >= 126) && (y <= 183))
			{
				waitForIt(170, 126, 300, 183);
				keypad_set = dc_link_keypad;
				page_num = keypad;
			}
			if ((y >= 200) && (y <= 238) && (x >= 1) && (x <= 79))
			{
				waitForIt(1, 200, 79, 238);
				page_num = main;
			}
			if ((y >= 200) && (y <= 238) && (x >= 81) && (x <= 159))
			{
				waitForIt(81, 200, 159, 238);
				page_num = secret1;
			}
			if ((y >= 200) && (y <= 238) && (x >= 161) && (x <= 239))
			{
				waitForIt(161, 200, 239, 238);
				page_num = offset;
			}
			if ((y >= 200) && (y <= 238) && (x >= 241) && (x <= 318))
			{
				waitForIt(241, 200, 318, 238);
				page_num = sett;
			}
			//    	  		}
			break;
		case offset_i:
			if ((y >= 200) && (y <= 238) && (x >= 1) && (x <= 79))
			{
				waitForIt(1, 200, 79, 238);
				page_num = main;
			}
			if ((y >= 200) && (y <= 238) && (x >= 241) && (x <= 318))
			{
				waitForIt(241, 200, 318, 238);
				page_num = secret1;
			}
			if ((y >= 0) && (y <= 10) && (x >= 0) && (x <= 10))
			{
				waitForIt(0, 0, 10, 10);
				dueFlashStorage.write(4094, 0);
				dueFlashStorage.write(4095, 0);
			}
			//    	  		}
			break;
		case keypad_i:
			if ((y >= 56) && (y <= 100))
			{
				if ((x >= 2) && (x <= 80))
				{ // Button: 7
					waitForIt(2, 56, 80, 100);
					updateStr('7');
				}
				if ((x >= 82) && (x <= 160))
				{ // Button: 8
					waitForIt(82, 56, 160, 100);
					updateStr('8');
				}
				if ((x >= 162) && (x <= 240))
				{ // Button: 9
					waitForIt(162, 56, 240, 100);
					updateStr('9');
				}
			}
			if ((y >= 102) && (y <= 146))
			{
				if ((x >= 2) && (x <= 80))
				{ // Button: 4
					waitForIt(2, 102, 80, 146);
					updateStr('4');
				}
				if ((x >= 82) && (x <= 160))
				{ // Button: 5
					waitForIt(82, 102, 160, 146);
					updateStr('5');
				}
				if ((x >= 162) && (x <= 240))
				{ // Button: 6
					waitForIt(162, 102, 240, 146);
					updateStr('6');
				}
			}
			if ((y >= 148) && (y <= 192))
			{ // Upper row
				if ((x >= 2) && (x <= 80))
				{ // Button: 1
					waitForIt(2, 148, 80, 192);
					updateStr('1');
				}
				if ((x >= 82) && (x <= 160))
				{ // Button: 2
					waitForIt(82, 148, 160, 192);
					updateStr('2');
				}
				if ((x >= 194) && (x <= 240))
				{ // Button: 3
					waitForIt(162, 148, 240, 192);
					updateStr('3');
				}
			}
			if ((y >= 194) && (y <= 238))
			{
				if ((x >= 2) && (x <= 160))
				{ // Button: 0
					waitForIt(2, 194, 160, 238);
					updateStr('0');
				}
				if ((x >= 162) && (x <= 240))
				{ // Button: delete
					waitForIt(162, 194, 240, 238);
					stCurrent[0] = '\0';
					stCurrentLen = 0;
					myGLCD.setColor(0, 0, 0);
					myGLCD.fillRect(160, 10, 319, 53);
				}
			}
			if ((x >= 242) && (x <= 317))
			{
				if ((y >= 148) && (y <= 238))
				{ // Button: esc
					waitForIt(242, 148, 317, 238);
					stCurrent[0] = '\0';
					stCurrentLen = 0;
					if ((keypad_set == battery_voltage_ref_keypad) || (keypad_set == battery_current_ref_keypad) || (keypad_set == output_voltage_ref_keypad)|| (keypad_set == discharge_set_bat_voltage_keypad))
						page_num = sett;
					else if ((keypad_set == input_voltage_keypad) || (keypad_set == input_current_keypad) || (keypad_set == inverter_voltage_keypad) || (keypad_set == inverter_current_keypad))
						page_num = secret1;
					else if ((keypad_set == battery_voltage_keypad) || (keypad_set == battery_current_keypad) || (keypad_set == output_current_keypad) || (keypad_set == dc_link_keypad))
						page_num = secret2;
					else if ((keypad_set == year_keypad) || (keypad_set == month_keypad) || (keypad_set == day_keypad) || (keypad_set == hour_keypad) || (keypad_set == minute_keypad) || (keypad_set == second_keypad))
						page_num = timeset;
				}
				if ((y >= 56) && (y <= 146))
				{ // Button: Enter
					waitForIt(242, 56, 317, 146);
					myGLCD.setFont(BigFont);
					if (stCurrentLen > 0)
					{
						for (x = 0; x < stCurrentLen + 1; x++)
						{
							if (keypad_set == battery_voltage_ref_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 280)
									input_set_data = 280;
								set_address = 0x33;
								tx_write();
							}
							if (keypad_set == battery_current_ref_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 20)
									input_set_data = 20;
								set_address = 0x32;
								tx_write();
							}
							if (keypad_set == output_voltage_ref_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 240)
									input_set_data = 240;
								set_address = 0x34;
								tx_write();
							}
							if (keypad_set == discharge_set_bat_voltage_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 240)
									input_set_data = 240;
								set_address = 0x35;
								tx_write();
							}
							if (keypad_set == input_voltage_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 500)
									input_set_data = 500;
								set_address = 0x3C;
								tx_write();
							}
							if (keypad_set == input_current_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 500)
									input_set_data = 500;
								set_address = 0x3D;
								tx_write();
							}
							if (keypad_set == inverter_voltage_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 500)
									input_set_data = 500;
								set_address = 0x41;
								tx_write();
							}
							if (keypad_set == inverter_current_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 500)
									input_set_data = 500;
								set_address = 0x42;
								tx_write();
							}
							if (keypad_set == battery_voltage_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 500)
									input_set_data = 500;
								set_address = 0x3F;
								tx_write();
							}
							if (keypad_set == battery_current_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 500)
									input_set_data = 500;
								set_address = 0x40;
								tx_write();
							}
							if (keypad_set == output_current_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 500)
									input_set_data = 500;
								set_address = 0x44;
								tx_write();
							}
							if (keypad_set == dc_link_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
								if (input_set_data > 500)
									input_set_data = 500;
								set_address = 0x3E;
								tx_write();
							}
							if (keypad_set == year_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
																  //      	  		       					  	 if (input_set_data >99) input_set_data=99;
																  //      	  		       					  	 rtc.adjust(DateTime(input_set_data, rtc_month, rtc_day, rtc_hour, rtc_minute, rtc_second));
							}
							if (keypad_set == month_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
																  //      	  		       					  	 if (input_set_data >12) input_set_data=12;
																  //      	  		       					  	 rtc.adjust(DateTime(rtc_year, input_set_data, rtc_day, rtc_hour, rtc_minute, rtc_second));
							}
							if (keypad_set == day_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
																  //      	  		       					  	 if (input_set_data >31) input_set_data=31;
																  //      	  		       					  	 rtc.adjust(DateTime(rtc_year, rtc_month, input_set_data, rtc_hour, rtc_minute, rtc_second));
							}
							if (keypad_set == hour_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
																  //      	  		       					  	 if (input_set_data >23) input_set_data=23;
																  //      	  		       					  	 rtc.adjust(DateTime(rtc_year, rtc_month, rtc_day, input_set_data, rtc_minute, rtc_second));
							}
							if (keypad_set == minute_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
																  //     	  		       					  	 if (input_set_data >59) input_set_data=59;
																  //     	  		       					  	 rtc.adjust(DateTime(rtc_year, rtc_month, rtc_day, rtc_hour, input_set_data, rtc_second));
							}
							if (keypad_set == second_keypad)
							{
								input_set_data = atoi(stCurrent); // char ���ڿ� int�� ��ȯ
																  //     	  		       					  	 if (input_set_data >59) input_set_data=59;
																  //     	  		       				  	 rtc.adjust(DateTime(rtc_year, rtc_month, rtc_day, rtc_minute, rtc_minute, input_set_data));
							}
						}
						stCurrent[0] = '\0';
						stCurrentLen = 0;
						if ((keypad_set == battery_voltage_ref_keypad) || (keypad_set == battery_current_ref_keypad) || (keypad_set == output_voltage_ref_keypad) || (keypad_set == discharge_set_bat_voltage_keypad))
							page_num = sett;
						else if ((keypad_set == input_voltage_keypad) || (keypad_set == input_current_keypad) || (keypad_set == inverter_voltage_keypad) || (keypad_set == inverter_current_keypad))
							page_num = secret1;
						else if ((keypad_set == battery_voltage_keypad) || (keypad_set == battery_current_keypad) || (keypad_set == output_current_keypad) || (keypad_set == dc_link_keypad))
							page_num = secret2;
						else if ((keypad_set == year_keypad) || (keypad_set == month_keypad) || (keypad_set == day_keypad) || (keypad_set == hour_keypad) || (keypad_set == minute_keypad) || (keypad_set == second_keypad))
							page_num = time_set;
					}
					else if (stCurrentLen == 0)
						buffer_empty();
					else
						buffer_full();
				}
			}
			//   				}
			break;
		case history_i:
			if ((y >= 200) && (y <= 238) && (x >= 1) && (x <= 79))
			{
				waitForIt(1, 200, 79, 238);
				page_num = main;
			}
			if ((y >= 200) && (y <= 238) && (x >= 81) && (x <= 159))
			{
				waitForIt(81, 200, 159, 238);
				eeprom_plus = 0;
				if (z == 0)
				{
					eeprom_plus = eeprom_address % 28;
					if (eeprom_plus == 0)
						z = z + 56 + eeprom_plus;
					else
						z = z + 28 + eeprom_plus;
				}
				else
					z = z + 56 + eeprom_plus;
				if (z < (eeprom_address + 1))
				{
					page_num = history_next;
				}
				else
				{
					z = eeprom_address - 28;
					myGLCD.setColor(30, 30, 30);
					myGLCD.drawRect(81, 200, 159, 238);
				}
			}
			if ((y >= 200) && (y <= 238) && (x >= 161) && (x <= 239))
			{
				waitForIt(161, 200, 239, 238);
				if (z > 0)
				{
					//									z+=4;
					y11 = 12;
					page_num = history_next;
				}
				else
				{
					myGLCD.setColor(30, 30, 30);
					myGLCD.drawRect(161, 200, 239, 238);
				}
			}
			if ((y >= 200) && (y <= 238) && (x >= 241) && (x <= 318))
			{
				waitForIt(241, 200, 318, 238);
				page_num = alarm1;
			}
			//        		}
			break;
		case timeset_i:
			if ((x >= 20) && (x <= 104) && (y >= 20) && (y <= 90))
			{
				waitForIt(20, 20, 104, 90);
				keypad_set = year_keypad;
				page_num = keypad;
			}
			if ((x >= 118) && (x <= 202) && (y >= 20) && (y <= 90))
			{ 
				waitForIt(118, 20, 202, 90);
				keypad_set = month_keypad;
				page_num = keypad;
			}
			if ((x >= 216) && (x <= 300) && (y >= 20) && (y <= 90))
			{ 
				waitForIt(216, 20, 300, 90);
				keypad_set = day_keypad;
				page_num = keypad;
			}
			if ((x >= 20) && (x <= 104) && (y >= 104) && (y <= 174))
			{ 
				waitForIt(20, 104, 104, 174);
				keypad_set = hour_keypad;
				page_num = keypad;
			}
			if ((x >= 118) && (x <= 202) && (y >= 104) && (y <= 174))
			{ 
				waitForIt(118, 104, 202, 174);
				keypad_set = minute_keypad;
				page_num = keypad;
			}
			if ((x >= 216) && (x <= 300) && (y >= 104) && (y <= 174))
			{ 
				waitForIt(216, 104, 300, 174);
				keypad_set = second_keypad;
				page_num = keypad;
			}
			if ((x >= 200) && (x <= 310) && (y >= 185) && (y <= 235))
			{ 
				waitForIt(200, 185, 310, 235);
				page_num = sett;
			}
			break;
		}
		leesh = 0;
	}

	else
		leesh++;
	if (leesh > 40)
	{
		leesh = 0;
		myTouch.InitTouch();
	}
}
void eeprom_history()
{

	myGLCD.setFont(Retro);
	myGLCD.setBackColor(255, 255, 255);
	myGLCD.setColor(240, 10, 30);
	if (z != 0)
	{
		for (int history_line = 0; history_line < 4; history_line++)
		{
			if (z < (eeprom_address + 1))
			{ 
				y11 += 22;
				myGLCD.setColor(50, 50, 50);
				see_eeprom = dueFlashStorage.read(z - 7);
				see_eeprom += 2000;
				myGLCD.print(String(see_eeprom), 5, y11);
				myGLCD.print("/", 40, y11);
				see_eeprom = dueFlashStorage.read(z - 6);
				myGLCD.print(String(see_eeprom), 50, y11);
				myGLCD.print("/", 67, y11);
				see_eeprom = dueFlashStorage.read(z - 5);
				myGLCD.print(String(see_eeprom), 80, y11);
				myGLCD.print("/", 99, y11);
				see_eeprom = dueFlashStorage.read(z - 4);
				myGLCD.print(String(see_eeprom), 127, y11);
				myGLCD.print(":", 146, y11);
				see_eeprom = dueFlashStorage.read(z - 3);
				myGLCD.print(String(see_eeprom), 154, y11);
				myGLCD.print(":", 173, y11);
				see_eeprom = dueFlashStorage.read(z - 2);
				myGLCD.print(String(see_eeprom), 181, y11);
				see_eeprom = dueFlashStorage.read(z - 1);
				y11 += 20;

				myGLCD.setColor(230, 0, 0);
				myGLCD.setFont(Retro);
				if (language == 1)
				{
					if (see_eeprom == 0)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_cu.RAW");
					else if (see_eeprom == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/vdc_ov.RAW");
					else if (see_eeprom == 2)
						myFiles.load(20, y11, 200, 16, "han/alarm/vdc_uv.RAW");
					else if (see_eeprom == 3)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_under.RAW");
					else if (see_eeprom == 4)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_over.RAW");
					else if (see_eeprom == 5)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_fre.RAW");
					else if (see_eeprom == 6)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_pri.RAW");
					else if (see_eeprom == 7)
						myFiles.load(20, y11, 200, 16, "han/alarm/util_line.RAW");
					else if (see_eeprom == 8)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_cu.RAW");
					else if (see_eeprom == 9)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_ov.RAW");
					else if (see_eeprom == 10)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_uv.RAW");
					else if (see_eeprom == 11)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_fault.RAW");
					else if (see_eeprom == 12)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_over_fault.RAW");
					else if (see_eeprom == 13)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_overtime.RAW");

					else if (see_eeprom == 15)
						myFiles.load(20, y11, 200, 16, "han/alarm/bypass_mode.RAW");

					else if (see_eeprom == 16)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_oc.RAW");
					else if (see_eeprom == 17)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_oc.RAW");
					else if (see_eeprom == 18)
						myFiles.load(20, y11, 200, 16, "han/alarm/link_ov.RAW");
					else if (see_eeprom == 19)
						myFiles.load(20, y11, 200, 16, "han/alarm/con_off.RAW");
					else if (see_eeprom == 20)
						myFiles.load(20, y11, 200, 16, "han/alarm/dc_off.RAW");

					else if (see_eeprom == 21)
						myFiles.load(20, y11, 200, 16, "han/alarm/con_gdu.RAW");
					else if (see_eeprom == 22)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_gdu.RAW");
					else if (see_eeprom == 23)
						myFiles.load(20, y11, 200, 16, "han/alarm/dcdc_gdu.RAW");
					else if (see_eeprom == 24)
						myFiles.load(20, y11, 200, 16, "han/alarm/common_gdu.RAW");
					else if (see_eeprom == 24)
						myFiles.load(20, y11, 200, 16, "han/alarm/earth_fault.RAW");
					else if (see_eeprom == 26)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_fuse.RAW");
					else if (see_eeprom == 27)
						myFiles.load(20, y11, 200, 16, "han/alarm/module_ot.RAW");
					else if (see_eeprom == 29)
						myFiles.load(20, y11, 200, 16, "han/alarm/eeprom_error.RAW");
				}
				else
				{
					if (see_eeprom == 0)
						myGLCD.print("Input current limit fault", 20, y11);
					else if (see_eeprom == 1)
						myGLCD.print("Vdc_link OV limit fault", 20, y11);
					else if (see_eeprom == 2)
						myGLCD.print("Vdc_link UV limit fault", 20, y11);
					else if (see_eeprom == 3)
						myGLCD.print("Input under voltage fault", 20, y11);
					else if (see_eeprom == 4)
						myGLCD.print("Input over voltage fault", 20, y11);
					else if (see_eeprom == 5)
						myGLCD.print("Input frequency fault", 20, y11);
					else if (see_eeprom == 6)
						myGLCD.print("Inverter frequency fault", 20, y11);
					else if (see_eeprom == 7)
						myGLCD.print("Utility line failure", 20, y11);
					else if (see_eeprom == 8)
						myGLCD.print("Battery current limit fault", 20, y11);
					else if (see_eeprom == 9)
						myGLCD.print("Battery OV limit fault", 20, y11);
					else if (see_eeprom == 10)
						myGLCD.print("Battery UV limit fault", 20, y11);
					else if (see_eeprom == 11)
						myGLCD.print("Inverter output voltage fault", 20, y11);
					else if (see_eeprom == 12)
						myGLCD.print("Inverter over load fault", 20, y11);
					else if (see_eeprom == 13)
						myGLCD.print("Inverter over load over time", 20, y11);

					else if (see_eeprom == 15)
						myGLCD.print("Bypass mode", 20, y11);

					else if (see_eeprom == 16)
						myGLCD.print("Input OC", 20, y11);
					else if (see_eeprom == 17)
						myGLCD.print("Inverter OC", 20, y11);
					else if (see_eeprom == 18)
						myGLCD.print("Vdc_link OV", 20, y11);
					else if (see_eeprom == 19)
						myGLCD.print("Converter OFF", 20, y11);
					else if (see_eeprom == 20)
						myGLCD.print("DC/DC OFF", 20, y11);

					else if (see_eeprom == 21)
						myGLCD.print("Converter GDU", 20, y11);
					else if (see_eeprom == 22)
						myGLCD.print("Inverter GDU", 20, y11);
					else if (see_eeprom == 23)
						myGLCD.print("DC/DC GDU", 20, y11);
					else if (see_eeprom == 24)
						myGLCD.print("Common GDU", 20, y11);
					else if (see_eeprom == 25)
						myGLCD.print("Earth fault", 20, y11);
					else if (see_eeprom == 26)
						myGLCD.print("Fuse Open", 20, y11);
					else if (see_eeprom == 27)
						myGLCD.print("Module OT", 20, y11);
					else if (see_eeprom == 29)
						myGLCD.print("EEPROM Error", 20, y11);
				}
				z = z - 7;
				if (z == 0)
					break;
				//						y11+=5
			}
		}
	}
	//	y11=18;
	page_num = history_i;
}
void eeprom_write()
{
	eeprom_address_h = dueFlashStorage.read(4094);
	eeprom_address_l = dueFlashStorage.read(4095);
	eeprom_address = (eeprom_address_l & 0x00ff) | ((eeprom_address_h << 8) & 0xff00);
	dueFlashStorage.write(eeprom_address, (rtc_year - 2000));
	dueFlashStorage.write(eeprom_address + 1, rtc_month);
	dueFlashStorage.write(eeprom_address + 2, rtc_day);
	dueFlashStorage.write(eeprom_address + 3, rtc_hour);
	dueFlashStorage.write(eeprom_address + 4, rtc_minute);
	dueFlashStorage.write(eeprom_address + 5, rtc_second);
	dueFlashStorage.write(eeprom_address + 6, eeprom_data);
	eeprom_address += 7;

	if (eeprom_address > 3499)
	{ //||(eeprom_address==(EEPROM.length()-1))||(eeprom_address==EEPROM.length()-2) 500개
		eeprom_address = 0;
	}
	eeprom_address_h = (eeprom_address >> 8) & 0xff;
	eeprom_address_l = (eeprom_address & 0xff);
	dueFlashStorage.write(4094, eeprom_address_h);
	dueFlashStorage.write(4095, eeprom_address_l);
	//	eeprom_address+=3;
}
void eeprom_set()
{
	if (hex_data[12] != ups_hw_state2)
	{

		//  	ups_hw_state1^=hex_data[5];
		//		hex_hw2=0x0F;
		hex_hw2 = ups_hw_state2;
		ups_hw_state2 ^= hex_data[12];
		for (int routine_check2 = 0; routine_check2 < 16; routine_check2++)
		{
			ups_hw_check2 = ups_hw_state2 & 0x0001;
			hex_hw2_h = hex_hw2 & 0x0001;
			if ((ups_hw_check2 == 1) && (hex_hw2_h == 0))
			{
				eeprom_data = routine_check2;
				eeprom_write();
				buzzer_switch = 1;
			}
			ups_hw_state2 = ups_hw_state2 >> 1;
			hex_hw2 = hex_hw2 >> 1;
		}
		ups_hw_state2 = hex_data[12];
	}
	if (hex_data[11] != ups_hw_state1)
	{
		hex_hw1 = ups_hw_state1;
		ups_hw_state1 ^= hex_data[11];
		for (int routine_check1 = 0; routine_check1 < 16; routine_check1++)
		{
			ups_hw_check1 = ups_hw_state1 & 0x0001;
			hex_hw1_h = hex_hw1 & 0x0001;
			if ((ups_hw_check1 == 1) && (hex_hw1_h == 0))
			{
				eeprom_data = routine_check1 + 16;
				eeprom_write();
				buzzer_switch = 1;
			}
			ups_hw_state1 = ups_hw_state1 >> 1;
			hex_hw1 = hex_hw1 >> 1;
		}
		ups_hw_state1 = hex_data[11];
	}
	buzz_snmp = hex_data[11] >> 12;
	buzz_snmp = buzz_snmp & 0x0001;
	if (buzz_snmp != buzz_snmp_old)
	{
		buzz_snmp_old = buzz_snmp;

		if ((0 != hex_data[11] & 0x3FFF) || (0 != hex_data[12] & 0xBFFF))
		{
			buzzer_click = 0;
			buzzer_switch ^= 1;
		}
	}
}
void serial_rx()
{
	//	Serial.println("start");
	if (Serial1.available())
	{
		serial_length = Serial1.available();
		int i, j;
		int rx_num, addr_num;
		for (i = 0; i < serial_length; i++)
		{
			rx[i] = Serial1.read();
		}
		while (Serial1.available() > 0)
		{
			Serial1.read();
		}
		if (rx[1] == 0x04)
		{
			if (serial_length == 11)
			{
				rx_num = 3;
				addr_num = 10;
			}
			else
			{
				if (page_num == meter_i)
				{
					rx_num = 12;
					addr_num = 30;
				}
				else if (page_num == main_i)
				{
					rx_num = 2;
					addr_num = 6;
				}
				else if (page_num == sett_i)
				{
					rx_num = 8;
					addr_num = 50;
				}
				else if (page_num == intro_i)
				{
					rx_num = 4;
					addr_num = 0;
				}
				else if ((page_num == secret1_i) || (page_num == secret2_i))
				{
					rx_num = 19;
					addr_num = 60;
				}
				else if (page_num == offset_i)
				{
					rx_num = 9;
					addr_num = 90;
				}
				else if (page_num == control_i)
				{
					rx_num = 6;
					addr_num = 15;
				}
			}
			crc_rx_data = MODBUS_CRC(rx, (rx_num * 2) + 3);
			crc_rx_h = (crc_rx_data >> 8) & 0xff;
			crc_rx_l = (crc_rx_data & 0xff);
			if ((rx[(rx_num * 2) + 3] == crc_rx_h) && (rx[(rx_num * 2) + 4] == crc_rx_l))
			{
				for (j = 0; j < rx_num; j++)
				{
					hex_data[j + addr_num] = ((short)(rx[j * 2 + 4] & 0x00ff) | (((short)rx[j * 2 + 3] << 8) & 0xff00));
				}
				hex_data[11] = hex_data[11] & 0x3FFF;
				hex_data[12] = hex_data[12] & 0xBFFF;
			}
			eeprom_set();
		}

		if (rx[1] == 0x10)
		{
			for (j = 0; j < 8; j++)
			{
			}
		}
		i = 0;
		j = 0;
	}
}

void background_menu()
{
	myGLCD.setColor(255, 31, 31);
	myGLCD.fillRect(29, 0, 174, 94); // OUTPUT VOLT
	myGLCD.setColor(0, 120, 6);
	myGLCD.fillRect(175, 0, 319, 94); // BATTURY VOLT
	myGLCD.setColor(0, 134, 189);
	myGLCD.fillRect(29, 95, 174, 189); // TIME SETUP
	myGLCD.setColor(255, 228, 0);
	myGLCD.fillRect(175, 95, 319, 189); // SYSTEM GAIN
	myGLCD.setColor(18, 18, 255);
	myGLCD.fillRect(29, 190, 174, 239); // HOME
	myGLCD.setColor(109, 109, 109);
	myGLCD.fillRect(175, 190, 319, 239); // ESC
	myGLCD.setColor(30, 30, 30);
	myGLCD.fillRect(0, 0, 28, 239); // MENU

	myGLCD.setColor(255, 255, 255);
	myGLCD.drawRect(29, 0, 174, 94);
	myGLCD.drawRect(175, 0, 319, 94);
	myGLCD.drawRect(29, 95, 174, 189);
	myGLCD.drawRect(175, 95, 319, 189);
	myGLCD.drawRect(29, 190, 174, 239);
	myGLCD.drawRect(175, 190, 319, 239);
	myGLCD.drawRect(0, 0, 28, 239);
}
void background_meter()
{
	myGLCD.fillScr(255, 255, 255); // White Color
	myGLCD.setColor(116, 175, 46);
	//	myGLCD.fillRoundRect (160, 10, 230, 50);
	myGLCD.fillRect(200, 185, 310, 235);
	myGLCD.setColor(70, 70, 70);
	myGLCD.fillRect(10, 30, 309, 58);
	myGLCD.fillRect(10, 88, 309, 116);
	myGLCD.fillRect(10, 146, 309, 174);
	myGLCD.fillRect(10, 204, 190, 232);
	myGLCD.setColor(0, 0, 0);
	//	myGLCD.drawRoundRect (160, 10, 230, 50);
	myGLCD.drawRect(200, 185, 310, 235);
	myGLCD.drawRect(10, 30, 309, 58);
	myGLCD.drawRect(10, 88, 309, 116);
	myGLCD.drawRect(10, 146, 309, 174);
	myGLCD.drawRect(10, 204, 190, 232);

	if ((hex_data[39] != 0) || (hex_data[40] != 0))
	{
		myGLCD.fillRect(247, 53, 248, 54);	 // hz point
		myGLCD.fillRect(247, 111, 248, 112); // hz point
		myGLCD.fillRect(247, 169, 248, 170); // hz point
	}
	myGLCD.setFont(BigFont);
	myGLCD.setColor(255, 255, 255);
	myGLCD.setBackColor(70, 70, 70);
	myGLCD.print("   V     A     Hz", 24, 37);
	myGLCD.print("   V     A     Hz", 24, 95);
	myGLCD.print("   V     A     Hz", 24, 153);
	myGLCD.print("   V     A", 24, 211);
	if (language == 1)
	{
		myFiles.load(30, 10, 64, 16, "han/measure/input_m.RAW");
		myFiles.load(30, 68, 64, 16, "han/measure/inverter_m.RAW");
		myFiles.load(30, 126, 64, 16, "han/measure/output_m.RAW");
		myFiles.load(30, 184, 64, 16, "han/measure/battery_m.RAW");
		myFiles.load(236, 201, 40, 20, "han/control/esc.RAW");
	}
	else
	{
		myGLCD.setBackColor(255, 255, 255);
		myGLCD.setColor(0, 0, 0);
		myGLCD.print("INPUT", 10, 10);
		myGLCD.print("INVERTER", 10, 68);
		myGLCD.print("OUTPUT", 10, 126);
		myGLCD.print("BATTERY", 10, 184);

		myGLCD.setBackColor(116, 175, 46);
		myGLCD.setColor(255, 255, 255);
		myGLCD.print("ESC", 232, 203);
	}
}
void background_gain()
{
	myGLCD.fillScr(255, 255, 255);
	myGLCD.setColor(60, 60, 60);
	myGLCD.fillRect(20, 30, 150, 57);
	myGLCD.fillRect(170, 30, 300, 57);
	myGLCD.fillRect(20, 126, 150, 153);
	myGLCD.fillRect(170, 126, 300, 153);
	myGLCD.setColor(0, 0, 0);
	myGLCD.fillRect(20, 58, 150, 60);
	myGLCD.fillRect(170, 58, 300, 60);
	myGLCD.fillRect(20, 154, 150, 156);
	myGLCD.fillRect(170, 154, 300, 156);
	myGLCD.drawRect(20, 30, 150, 87);
	myGLCD.drawRect(170, 30, 300, 87);
	myGLCD.drawRect(20, 126, 150, 183);
	myGLCD.drawRect(170, 126, 300, 183);

	/*****menu******/
	myGLCD.setColor(248, 111, 30);
	myGLCD.fillRect(1, 200, 79, 238);
	myGLCD.fillRect(81, 200, 159, 238);
	myGLCD.fillRect(161, 200, 239, 238);
	myGLCD.fillRect(241, 200, 318, 238);
	myGLCD.setColor(30, 30, 30);
	myGLCD.drawRect(1, 200, 79, 238);
	myGLCD.drawRect(81, 200, 159, 238);
	myGLCD.drawRect(161, 200, 239, 238);
	myGLCD.drawRect(241, 200, 318, 238);
}
void background_alarm()
{
	myGLCD.fillScr(255, 255, 255); // White Color
	myGLCD.setColor(50, 50, 50);
	myGLCD.drawLine(0, 26, 319, 26);
	myGLCD.setColor(116, 175, 46);
	myGLCD.fillRect(0, 0, 319, 25);

	/*****menu******/
	myGLCD.setColor(248, 111, 30);
	myGLCD.fillRect(1, 200, 79, 238);
	myGLCD.fillRect(81, 200, 159, 238);
	myGLCD.fillRect(161, 200, 239, 238);
	myGLCD.fillRect(241, 200, 318, 238);
	myGLCD.setColor(30, 30, 30);
	myGLCD.drawRect(1, 200, 79, 238);
	myGLCD.drawRect(81, 200, 159, 238);
	myGLCD.drawRect(161, 200, 239, 238);
	myGLCD.drawRect(241, 200, 318, 238);
	if (language == 1)
	{
		myFiles.load(112, 5, 74, 16, "han/alarm/alarm_state.RAW");
		myFiles.load(24, 212, 32, 16, "han/alarm/history_btn.RAW");
		myFiles.load(104, 212, 32, 16, "han/alarm/prew_btn.RAW");
		myFiles.load(184, 212, 32, 16, "han/alarm/next_btn.RAW");
		myFiles.load(264, 212, 32, 16, "han/alarm/esc_btn.RAW");
	}
	else
	{
		myGLCD.setColor(255, 255, 255);
		myGLCD.setFont(BigFont);
		myGLCD.setBackColor(116, 175, 46);
		myGLCD.print("Alarm State", 53, 5);

		myGLCD.setFont(Retro);
		myGLCD.setColor(0, 0, 0);
		myGLCD.setBackColor(255, 255, 255);
		myGLCD.setBackColor(248, 111, 30);
		myGLCD.print("HISTORY", 12, 212);

		myGLCD.print("PREW", 104, 212);
		myGLCD.print("NEXT", 184, 212);
		myGLCD.print("ESC", 268, 212);
	}
}
void background_history()
{
	myGLCD.fillScr(255, 255, 255); // White Color
	/*****menu******/
	myGLCD.setColor(50, 50, 50);
	myGLCD.drawLine(0, 26, 319, 26);
	myGLCD.setColor(116, 175, 46);
	myGLCD.fillRect(0, 0, 319, 25);

	/*****menu******/
	myGLCD.setColor(248, 111, 30);
	myGLCD.fillRect(1, 200, 79, 238);
	myGLCD.fillRect(81, 200, 159, 238);
	myGLCD.fillRect(161, 200, 239, 238);
	myGLCD.fillRect(241, 200, 318, 238);
	myGLCD.setColor(30, 30, 30);
	myGLCD.drawRect(1, 200, 79, 238);
	myGLCD.drawRect(81, 200, 159, 238);
	myGLCD.drawRect(161, 200, 239, 238);
	myGLCD.drawRect(241, 200, 318, 238);

	if (language == 1)
	{
		myFiles.load(112, 5, 74, 16, "han/alarm/history_event.RAW");
		myFiles.load(24, 212, 32, 16, "han/alarm/main_btn.RAW");
		myFiles.load(104, 212, 32, 16, "han/alarm/prew_btn.RAW");
		myFiles.load(184, 212, 32, 16, "han/alarm/next_btn.RAW");
		myFiles.load(264, 212, 32, 16, "han/alarm/esc_btn.RAW");
	}
	else
	{
		myGLCD.setColor(255, 255, 255);
		myGLCD.setFont(BigFont);
		myGLCD.setBackColor(116, 175, 46);
		myGLCD.print("History Event", 53, 5);

		myGLCD.setFont(Retro);
		myGLCD.setColor(0, 0, 0);
		myGLCD.setBackColor(255, 255, 255);
		myGLCD.setBackColor(248, 111, 30);
		myGLCD.print("MAIN", 24, 212);
		myGLCD.print("PREW", 104, 212);
		myGLCD.print("NEXT", 184, 212);
		myGLCD.print("ESC", 268, 212);
	}
}
void buzz_set()
{
	alarm_data1 = hex_data[12];
	//		alarm_data1= 0xFFFF;
	alarm_data2 = hex_data[11];
	alarm_count = 0;
	alarm_number1 = alarm_data1 & 0xBFFF;
	for (alarm_number_1i = 0; alarm_number1 != 0; alarm_number_1i++)
	{ // �˶� ���� Ȯ��
		alarm_number1 &= (alarm_number1 - 1);
	}
	alarm_number2 = alarm_data2 & 0x3FFF; // ON OFF ����ġ üũ ��
	for (alarm_number_2i = 0; alarm_number2 != 0; alarm_number_2i++)
	{ // �˶� ���� Ȯ��
		alarm_number2 &= (alarm_number2 - 1);
	}
	alarm_number = alarm_number1 + alarm_number2;
	alarm_number_i = alarm_number_1i + alarm_number_2i;
}

void alarm_set()
{
	myGLCD.setFont(Retro);
	myGLCD.setBackColor(255, 255, 255);
	myGLCD.setColor(255, 0, 0);
	alarm_data1 = hex_data[12];
	//		alarm_data1=0xFFFF;
	alarm_data2 = hex_data[11];
	alarm_count = 0;
	k = 0;
	y11 = 40;
	alarm_number1 = alarm_data1 & 0xBFFF;
	for (alarm_number_1i = 0; alarm_number1 != 0; alarm_number_1i++)
	{ // �˶� ���� Ȯ��
		alarm_number1 &= (alarm_number1 - 1);
	}
	//	for(i=0;i<alarm_number_i;i++)
	for (i = 0; i < 16; i++)
	{
		if (y11 > 193)
			break;
		alarm_check = alarm_data1 & 0x0001;
		if (alarm_check == 1)
		{
			if (k == 0)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_cu.RAW");
					else
						myGLCD.print("Input current limit fault     ", 20, y11);
					y11 = y11 + 22;
				}
				else if ((page_num == alarm2_i) || (page_num == alarm3_i))
					alarm_count++;
			}
			if (k == 1)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/vdc_ov.RAW");
					else
						myGLCD.print("Vdc_link OV limit fault       ", 20, y11);
					y11 = y11 + 22;
				}
				else if ((page_num == alarm2_i) || (page_num == alarm3_i))
					alarm_count++;
			}
			if (k == 2)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/vdc_uv.RAW");
					else
						myGLCD.print("Vdc_link UV limit fault       ", 20, y11);
					y11 = y11 + 22;
				}
				else if ((page_num == alarm2_i) || (page_num == alarm3_i))
					alarm_count++;
			}
			if (k == 3)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_under.RAW");
					else
						myGLCD.print("Input under voltage fault     ", 20, y11);
					y11 = y11 + 22;
				}
				else if ((page_num == alarm2_i) || (page_num == alarm3_i))
					alarm_count++;
			}
			if (k == 4)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_over.RAW");
					else
						myGLCD.print("Input over voltage fault      ", 20, y11);
					y11 = y11 + 22;
				}
				else if ((page_num == alarm2_i) || (page_num == alarm3_i))
					alarm_count++;
			}
			if (k == 5)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_fre.RAW");
					else
						myGLCD.print("Input frequency fault         ", 20, y11);
					y11 = y11 + 22;
				}
				else if ((page_num == alarm2_i) || (page_num == alarm3_i))
					alarm_count++;
			}
			if (k == 6)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_pri.RAW");
					else
						myGLCD.print("Battery pri under voltage     ", 20, y11);
					y11 = y11 + 22;
				}
				else if ((page_num == alarm2_i) || (page_num == alarm3_i))
					alarm_count++;
			}
			if (k == 7)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/util_line.RAW");
					else
						myGLCD.print("Utility line failure          ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/util_line.RAW");
						else
							myGLCD.print("Utility line failure          ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
					alarm_count++;
			}
			if (k == 8)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_cu.RAW");
					else
						myGLCD.print("Battery current limit fault   ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/bat_cu.RAW");
						else
							myGLCD.print("Battery current limit fault   ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
					alarm_count++;
			}
			if (k == 9)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_ov.RAW");
					else
						myGLCD.print("Battery OV limit fault        ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/bat_ov.RAW");
						else
							myGLCD.print("Battery OV limit fault        ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
					alarm_count++;
			}
			if (k == 10)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_uv.RAW");
					else
						myGLCD.print("Battery UV limit fault        ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/bat_uv.RAW");
						else
							myGLCD.print("Battery UV limit fault        ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
					alarm_count++;
			}
			if (k == 11)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_fault.RAW");
					else
						myGLCD.print("Inverter output voltage fault ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/inv_fault.RAW");
						else
							myGLCD.print("Inverter output voltage fault ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
					alarm_count++;
			}
			if (k == 12)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_over_fault.RAW");
					else
						myGLCD.print("Inverter over load fault      ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/inv_over_fault.RAW");
						else
							myGLCD.print("Inverter over load fault      ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
					alarm_count++;
			}
			if (k == 13)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_overtime.RAW");
					else
						myGLCD.print("Inverter over load over time  ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/inv_overtime.RAW");
						else
							myGLCD.print("Inverter over load over time  ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
					alarm_count++;
			}
			if (k == 15)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/bypass_mode.RAW");
					else
						myGLCD.print("Bypass mode                   ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/bypass_mode.RAW");
						else
							myGLCD.print("Bypass mode                   ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/bypass_mode.RAW");
						else
							myGLCD.print("Bypass mode                   ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
		}
		alarm_data1 = alarm_data1 >> 1;
		k++;
	}
	k = 0;
	alarm_number2 = alarm_data2 & 0x3FFF; // ON OFF ����ġ üũ ��
	for (alarm_number_2i = 0; alarm_number2 != 0; alarm_number_2i++)
	{ // �˶� ���� Ȯ��
		alarm_number2 &= (alarm_number2 - 1);
	}
	//	for(i=0;i<alarm_number_i;i++)
	for (i = 0; i < 16; i++)
	{
		if (y11 > 193)
			break;
		alarm_check = alarm_data2 & 0x0001;
		if (alarm_check == 1)
		{
			if (k == 0)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/in_oc.RAW");
					else
						myGLCD.print("Input OC                      ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/in_oc.RAW");
						else
							myGLCD.print("Input OC                      ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/in_oc.RAW");
						else
							myGLCD.print("Input OC                      ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 1)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_oc.RAW");
					else
						myGLCD.print("Inverter OC                   ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/inv_oc.RAW");
						else
							myGLCD.print("Inverter OC                   ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/inv_oc.RAW");
						else
							myGLCD.print("Inverter OC                   ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 2)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/link_ov.RAW");
					else
						myGLCD.print("Vdc link OV                   ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/link_ov.RAW");
						else
							myGLCD.print("Vdc link OV                   ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/link_ov.RAW");
						else
							myGLCD.print("Vdc link OV                   ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 3)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/con_off.RAW");
					else
						myGLCD.print("Converter OFF                 ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/con_off.RAW");
						else
							myGLCD.print("Converter OFF                 ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/con_off.RAW");
						else
							myGLCD.print("Converter OFF                 ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 4)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/dc_off.RAW");
					else
						myGLCD.print("DC/DC OFF                     ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/dc_off.RAW");
						else
							myGLCD.print("DC/DC OFF                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/dc_off.RAW");
						else
							myGLCD.print("DC/DC OFF                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 5)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/con_gdu.RAW");
					else
						myGLCD.print("Converter GDU                     ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/con_gdu.RAW");
						else
							myGLCD.print("Converter GDU                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/con_gdu.RAW");
						else
							myGLCD.print("Converter GDU                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 6)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/inv_gdu.RAW");
					else
						myGLCD.print("Inverter GDU                  ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/inv_gdu.RAW");
						else
							myGLCD.print("Inverter GDU                  ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/inv_gdu.RAW");
						else
							myGLCD.print("Inverter GDU                  ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 7)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/dcdc_gdu.RAW");
					else
						myGLCD.print("DC/DC GDU                     ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/dcdc_gdu.RAW");
						else
							myGLCD.print("DC/DC GDU                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/dcdc_gdu.RAW");
						else
							myGLCD.print("DC/DC GDU                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 8)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/common_gdu.RAW");
					else
						myGLCD.print("Common GDU                    ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/common_gdu.RAW");
						else
							myGLCD.print("Common GDU                    ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/common_gdu.RAW");
						else
							myGLCD.print("Common GDU                    ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 9)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/earth_fault.RAW");
					else
						myGLCD.print("Earth_fault                   ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/earth_fault.RAW");
						else
							myGLCD.print("Earth_fault                   ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/earth_fault.RAW");
						else
							myGLCD.print("Earth_fault                   ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 10)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/bat_fuse.RAW");
					else
						myGLCD.print("Fuse Open                     ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/bat_fuse.RAW");
						else
							myGLCD.print("Fuse Open                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/bat_fuse.RAW");
						else
							myGLCD.print("Fuse Open                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
			if (k == 11)
			{
				if (page_num == alarm1_i)
				{
					if (language == 1)
						myFiles.load(20, y11, 200, 16, "han/alarm/module_ot.RAW");
					else
						myGLCD.print("Module OT                     ", 20, y11);
					y11 = y11 + 22;
				}
				else if (page_num == alarm2_i)
				{
					alarm_count++;
					if ((alarm_count > 7) && (alarm_count < 15))
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/module_ot.RAW");
						else
							myGLCD.print("Module OT                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
				else if (page_num == alarm3_i)
				{
					alarm_count++;
					if (alarm_count > 14)
					{
						if (language == 1)
							myFiles.load(20, y11, 200, 16, "han/alarm/module_ot.RAW");
						else
							myGLCD.print("Module OT                     ", 20, y11);
						y11 = y11 + 22;
					}
				}
			}
		}
		alarm_data2 = alarm_data2 >> 1;
		k++;
	}
	alarm_number = alarm_number1 + alarm_number2;
	alarm_number_i = alarm_number_1i + alarm_number_2i;

	y1_delete = (alarm_number_i * 22) + 40;
	if (y1_delete > 190)
		y1_delete = 190;
	myGLCD.setColor(255, 255, 255);
	myGLCD.fillRect(20, y1_delete, 300, 190);
}
void drawButtons()
{
	myGLCD.fillScr(0, 0, 0);
	myGLCD.setFont(BigFont);
	myGLCD.setBackColor(190, 190, 190);
	for (x = 0; x < 3; x++)
	{
		myGLCD.setColor(190, 190, 190);
		myGLCD.fillRect(2 + (x * 80), 148, 80 + (x * 80), 192);
		myGLCD.setColor(0, 0, 0);
		myGLCD.printNumI(x + 1, 33 + (x * 80), 163);
		myGLCD.setColor(255, 255, 255);
		myGLCD.drawRect(2 + (x * 80), 148, 80 + (x * 80), 192);
	}
	for (x = 0; x < 3; x++)
	{
		myGLCD.setColor(190, 190, 190);
		myGLCD.fillRect(2 + (x * 80), 102, 80 + (x * 80), 146);
		myGLCD.setColor(0, 0, 0);
		myGLCD.printNumI(x + 4, 33 + (x * 80), 117);
		myGLCD.setColor(255, 255, 255);
		myGLCD.drawRect(2 + (x * 80), 102, 80 + (x * 80), 146);
	}
	for (x = 0; x < 3; x++)
	{
		myGLCD.setColor(190, 190, 190);
		myGLCD.fillRect(2 + (x * 80), 56, 80 + (x * 80), 100);
		myGLCD.setColor(0, 0, 0);
		myGLCD.printNumI(x + 7, 33 + (x * 80), 71);
		myGLCD.setColor(255, 255, 255);
		myGLCD.drawRect(2 + (x * 80), 56, 80 + (x * 80), 100);
	}
	myGLCD.setColor(190, 190, 190);
	myGLCD.fillRect(2, 194, 160, 238);
	myGLCD.fillRect(162, 194, 240, 238);
	myGLCD.setColor(242, 90, 0);
	myGLCD.fillRect(242, 56, 317, 146);
	myGLCD.fillRect(242, 148, 317, 238);
	myGLCD.setColor(0, 0, 0);
	myGLCD.print("0", 33, 209);
	myGLCD.print("DEL", 176, 209);
	myGLCD.setBackColor(242, 90, 0);
	myGLCD.setColor(255, 255, 255);
	myGLCD.drawRect(2, 194, 160, 238);
	myGLCD.drawRect(162, 194, 240, 238);
	myGLCD.drawRect(242, 56, 317, 146);
	myGLCD.drawRect(242, 148, 317, 238);
	myGLCD.print("SET", 259, 93);
	myGLCD.print("ESC", 259, 185);
}

void updateStr(int val)
{
	if (stCurrentLen < 3)
	{
		stCurrent[stCurrentLen] = val;
		stCurrent[stCurrentLen + 1] = '\0';
		stCurrentLen++;
		myGLCD.setColor(255, 255, 255);
		myGLCD.setBackColor(0, 0, 0);
		myGLCD.setFont(Segment);
		// 	    myGLCD.print(stCurrent, (RIGHT-(stCurrentLen*24)), 18);
		myGLCD.print(stCurrent, RIGHT, 18);
	}
	else
	{
		buffer_full();
	}
}
void buffer_empty()
{
	myGLCD.setFont(BigFont);
	myGLCD.setBackColor(0, 0, 0);
	myGLCD.setColor(255, 0, 0);
	myGLCD.print("BUFFER EMPTY", 10, 25);
	delay(500);
	myGLCD.print("            ", 10, 25);
	delay(500);
	myGLCD.print("BUFFER EMPTY", 10, 25);
	delay(500);
	myGLCD.print("            ", 10, 25);
	myGLCD.setColor(0, 0, 0);
	myGLCD.fillRect(160, 10, 319, 53);
	stCurrentLen = 0;
}
void buffer_full()
{
	myGLCD.setFont(BigFont);
	myGLCD.setColor(255, 0, 0);
	myGLCD.print("BUFFER FULL", 10, 25);
	delay(500);
	myGLCD.print("            ", 10, 25);
	delay(500);
	myGLCD.print("BUFFER FULL", 10, 25);
	delay(500);
	myGLCD.print("            ", 10, 25);
	myGLCD.setColor(0, 0, 0);
	myGLCD.fillRect(160, 10, 319, 53);
	stCurrentLen = 0;
}

void position(int address, int x1_check, int y1_check)
{
	int color, backcolor;
	if (page_num == meter_i)
	{
		color = 255;
		backcolor = 70;
	}
	if ((page_num == main_i) || (page_num == control_i))
	{
		color = 0;
		backcolor = 255;
	}
	if ((page_num == secret1_i) || (page_num == secret2_i))
	{
		color = 255;
		backcolor = 70;
	}
	myGLCD.setColor(color, color, color);
	myGLCD.setBackColor(backcolor, backcolor, backcolor);
	if ((hex_data[address] > 99) && (hex_data[address] < 1000))
	{
		myGLCD.print(String(hex_data[address]), x1_check, y1_check);
	}
	if ((hex_data[address] > 9) && (hex_data[address] < 100))
	{
		myGLCD.print(String(hex_data[address]), x1_check + 16, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 15, y1_check + 15);
	}
	if (hex_data[address] < 10)
	{
		myGLCD.print(String(hex_data[address]), x1_check + 32, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 31, y1_check + 15);
	}
}
void position2(int address, int x1_check, int y1_check)
{
	int color, backcolor;
	if ((page_num == secret1_i) || (page_num == secret2_i))
	{
		color = 255;
		backcolor = 255;
	}
	if (page_num == intro_i)
	{
		color = 220;
		backcolor = 230;
	}
	if (page_num == control_i)
	{
		color = 255;
		backcolor = 255;
	}
	if (page_num == sett_i)
	{
		color = 220;
		backcolor = 255;
	}
	myGLCD.setColor(color, 0, 0);
	myGLCD.setBackColor(backcolor, backcolor, backcolor);
	if ((hex_data[address] > 99) && (hex_data[address] < 1000))
	{
		myGLCD.print(String(hex_data[address]), x1_check, y1_check);
	}
	if ((hex_data[address] > 9) && (hex_data[address] < 100))
	{
		myGLCD.print(String(hex_data[address]), x1_check + 16, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 15, y1_check + 15);
	}
	if (hex_data[address] < 10)
	{
		myGLCD.print(String(hex_data[address]), x1_check + 32, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 31, y1_check + 15);
	}
}
void position3(int address, int x1_check, int y1_check)
{
	int color, backcolor;
	color = 0;
	backcolor = 255;

	myGLCD.setColor(255, color, color);
	myGLCD.setBackColor(backcolor, backcolor, backcolor);
	if ((hex_data[address] > 999) && (hex_data[address] < 10000))
	{
		myGLCD.print(String(hex_data[address]), x1_check, y1_check);
	}
	if ((hex_data[address] > 99) && (hex_data[address] < 1000))
	{
		myGLCD.print(String(hex_data[address]), x1_check + 16, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 15, y1_check + 15);
	}
	if ((hex_data[address] > 9) && (hex_data[address] < 100))
	{
		myGLCD.print(String(hex_data[address]), x1_check + 32, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 31, y1_check + 15);
	}
	if (hex_data[address] < 10)
	{
		myGLCD.print(String(hex_data[address]), x1_check + 48, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 47, y1_check + 15);
	}
}
void segment_position(int address, int x1_check, int y1_check)
{
	myGLCD.setFont(Segment);
	int color, backcolor;
	if (page_num == sett_i)
	{
		color = 220;
		backcolor = 255;
	}
	myGLCD.setColor(color, 0, 0);
	myGLCD.setBackColor(backcolor, backcolor, backcolor);
	if ((hex_data[address] > 99) && (hex_data[address] < 1000))
	{
		myGLCD.print(String(hex_data[address]), x1_check, y1_check);
	}
	if ((hex_data[address] > 9) && (hex_data[address] < 100))
	{
		myGLCD.print(String(hex_data[address]), x1_check + 24, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 23, y1_check + 31);
	}
	if (hex_data[address] < 10)
	{
		myGLCD.print(String(hex_data[address]), x1_check + 48, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 47, y1_check + 31);
	}
}
void time_segment_position(int address, int x1_check, int y1_check)
{
	myGLCD.setFont(Segment);
	int color = 255, backcolor = 50;
	myGLCD.setColor(color, color, color);
	myGLCD.setBackColor(backcolor, backcolor, backcolor);
	if ((address > 9) && (address < 100))
	{
		myGLCD.print(String(address), x1_check, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check, y1_check);
	}
	if (address < 10)
	{
		myGLCD.print(String(address), x1_check + 24, y1_check);
		myGLCD.setColor(backcolor, backcolor, backcolor);
		myGLCD.fillRect(x1_check, y1_check, x1_check + 23, y1_check + 31);
	}
}
void waitForIt(int x1, int y11, int x2, int y2)
{
	myGLCD.setColor(255, 0, 0);
	myGLCD.drawRect(x1, y11, x2, y2);
	while (myTouch.dataAvailable())
		myTouch.read();
	myGLCD.setColor(255, 255, 255);
	myGLCD.drawRect(x1, y11, x2, y2);
}
void ac_model(int x_line, int y_line)
{
	myGLCD.setColor(255, 255, 255);
	myGLCD.drawCircle(x_line, y_line, 5);
	myGLCD.drawCircle(x_line, y_line, 4);
	myGLCD.drawCircle((x_line + 10), y_line, 5);
	myGLCD.drawCircle((x_line + 10), y_line, 4);
	if (ac_onoff == 0)
		myGLCD.setColor(60, 60, 60);
	else if (ac_onoff == 1)
		myGLCD.setColor(22, 108, 171);
	myGLCD.fillRect((x_line - 5), (y_line + 2), (x_line + 4), (y_line + 5));
	myGLCD.fillRect((x_line + 6), (y_line - 5), (x_line + 15), (y_line - 2));
}
void setup()
{
	Serial.begin(9600);
	Serial1.begin(9600);
	Serial.println("ok 1");
	myGLCD.InitLCD();
	myGLCD.clrScr();
	rtc.begin();

	pinMode(8, OUTPUT); // buzz
	int eep_check = dueFlashStorage.read(4096);
	if (eep_check = !4)
	{
		for (int eep_set = 0; eep_set < 4096; eep_set++)
		{
			dueFlashStorage.write(eep_set, 0);
		}
		dueFlashStorage.write(4096, 4);
		dueFlashStorage.write(4097, 0);
		dueFlashStorage.write(4098, 0);
	}
	intro_type = dueFlashStorage.read(4098);
	Serial.print("ok 2 intro_type=");
	Serial.println(intro_type);
	if((intro_type == 1) || (intro_type == 3)) {
		// flash.begin() hangs if SPI flash is missing/busy (BUSY_TIMEOUT ~16 min).
		// This sketch never uses flash after begin(); keep CS high so it stays off the SPI bus.
		pinMode(87, OUTPUT);
		digitalWrite(87, HIGH);
		if (!sd.begin(SD_CHIP_SELECT, SPI_HALF_SPEED)) {
			Serial.println("SD HALF fail, retry QUARTER");
			if (!sd.begin(SD_CHIP_SELECT, SPI_QUARTER_SPEED)) {
				Serial.println("SD begin fail");
			}
		}
	}

	myGLCD.fillScr(255, 255, 255);
	myGLCD.setColor(0, 0, 0);
	myGLCD.setBackColor(255, 255, 255);
	myGLCD.setFont(BigFont);
	myTouch.InitTouch();
	myTouch.setPrecision(PREC_MEDIUM);
	myGLCD.print("I", 40, 90);
	delay(300);
	myGLCD.print("F", 85, 90);
	delay(300);
	myGLCD.print("T", 130, 90);
	delay(300);
	myGLCD.print("E", 175, 90);
	delay(300);
	myGLCD.print("C", 220, 90);
	delay(300);
	myGLCD.print("H", 265, 90);
	delay(300);
	myGLCD.setFont(Retro);
	myGLCD.setColor(30, 30, 30);
	myGLCD.print("I", 56, 160);
	delay(20);
	myGLCD.print("N", 64, 160);
	delay(20);
	myGLCD.print("F", 72, 160);
	delay(20);
	myGLCD.print("I", 80, 160);
	delay(20);
	myGLCD.print("N", 88, 160);
	delay(20);
	myGLCD.print("I", 96, 160);
	delay(20);
	myGLCD.print("T", 104, 160);
	delay(20);
	myGLCD.print("E", 112, 160);
	delay(20);
	myGLCD.print("F", 128, 160);
	delay(20);
	myGLCD.print("U", 136, 160);
	delay(20);
	myGLCD.print("T", 144, 160);
	delay(20);
	myGLCD.print("U", 152, 160);
	delay(20);
	myGLCD.print("R", 160, 160);
	delay(20);
	myGLCD.print("E", 168, 160);
	delay(20);
	myGLCD.print("T", 184, 160);
	delay(20);
	myGLCD.print("E", 192, 160);
	delay(20);
	myGLCD.print("C", 200, 160);
	delay(20);
	myGLCD.print("H", 208, 160);
	delay(20);
	myGLCD.print("N", 216, 160);
	delay(20);
	myGLCD.print("O", 224, 160);
	delay(20);
	myGLCD.print("L", 232, 160);
	delay(20);
	myGLCD.print("O", 240, 160);
	delay(20);
	myGLCD.print("G", 248, 160);
	delay(20);
	myGLCD.print("Y", 256, 160);
	delay(1000);
	myTouch.calibrateRead();
	eeprom_address_h = dueFlashStorage.read(4094);
	eeprom_address_l = dueFlashStorage.read(4095);
	eeprom_address = (eeprom_address_l & 0x00ff) | ((eeprom_address_h << 8) & 0xff00);
	if (eeprom_address > 3499)
	{
		dueFlashStorage.write(4094, 0);
		dueFlashStorage.write(4095, 0);
	}
	Serial.println("Serial bps 9600 start");
	Timer3.attachInterrupt(interrupt).start(1000);
	setupWatchDog();
	
	language = dueFlashStorage.read(4097);
	
	//		language=0;
}

void loop()
{
	WatchDogClear();
	buzz_set();
	if (alarm_number_i == 0)
	{
		buzzer_switch = 0;
		buzzer_click = 0;
	}
	//		myTouch.InitTouch();
	rtc_year = (rtc.getYear());
	rtc_month = (rtc.getMonth());
	rtc_day = (rtc.getDay());
	rtc_hour = (rtc.getHours());
	rtc_minute = (rtc.getMinutes());
	rtc_second = (rtc.getSeconds());
	if (touch_check == 1)
	{
		touch_check = 0;
		touch_read();
	}

	if (timer300ms > 299)
	{
		timer300ms = 0;
		tx_toggle = tx_toggle ^ 1;
		if (tx_toggle == 0)
		{
			alarm_tx();
		}
		else if (tx_toggle == 1)
		{
			data_tx();
		}
		//  			Serial.println(tx_toggle);
	}

	switch (page_num)
	{
	case intro_set:
		if((intro_type == 0) || (intro_type == 255)) {
			myGLCD.fillScr(255, 255, 255); // White Color
			myGLCD.setFont(BigFont);
			myGLCD.setColor(0, 0, 0);
			myGLCD.setBackColor(255, 255, 255);
			myGLCD.print("STANDARD", 15, 50);
			myGLCD.print("SCANJET", 184, 50);
			myGLCD.print("TPS", 60, 170);
			myGLCD.print("ENGLISH", 184, 170);
			myGLCD.drawLine(0, 120, 319, 120);
			myGLCD.drawLine(160, 0, 160, 239);
			page_num = intro_set_i;
		}
		else page_num = intro;
		break;
	case intro_set_i:

		break;
	case intro:
		myGLCD.fillScr(255, 255, 255); // White Color
		if (intro_type == 2) {
			myGLCD.drawBitmap(35, 15, 250, 99, scanjet);
			language = 0;
		}
		else if (intro_type == 3) {
			myGLCD.drawBitmap(5, 40, 310, 70, tps);
		}		
		else if (intro_type == 4) {
			language = 0;
		}	
		myGLCD.setColor(230, 230, 230);
		myGLCD.fillRect(0, 125, 319, 175);
		//						myGLCD.fillRect (80, 35, 95, 50);
		myGLCD.setFont(BigFont);
		myGLCD.setColor(0, 0, 0);
		myGLCD.setBackColor(230, 230, 230);
		myGLCD.print("KVA", 90, 130);
		myGLCD.print("CELL", 210, 130);
		//  					myGLCD.print("IN    V OUT    V", 30, 155);

		myGLCD.print("V", 126, 155);

		myGLCD.print("V", 270, 155);

		myGLCD.drawBitmap(89, 185, 141, 45, enter);

		if ((intro_type == 1) || (intro_type == 3))
		{
			if (language == 1)
			{
				myFiles.load(30, 155, 32, 16, "han/intro/in.RAW");
				myFiles.load(166, 155, 32, 16, "han/intro/out.RAW");
				myGLCD.setColor(206, 15, 33);
				myGLCD.fillRect(270, 0, 319, 24);
				myGLCD.setColor(120, 120, 120);
				myGLCD.fillRect(220, 0, 269, 24);
				myFiles.load(229, 5, 32, 16, "han/intro/english.RAW");
				myFiles.load(279, 5, 32, 16, "han/intro/korean.RAW");
			}
			else
			{
				myGLCD.setColor(120, 120, 120);
				myGLCD.fillRect(270, 0, 319, 24);
				myGLCD.setColor(206, 15, 33);
				myGLCD.fillRect(220, 0, 269, 24);
				myGLCD.setColor(255, 255, 255);
				myGLCD.setBackColor(206, 15, 33);
				myGLCD.print("EN", 229, 5);
				myGLCD.setBackColor(120, 120, 120);
				myGLCD.print("KO", 279, 5);

				myGLCD.setColor(0, 0, 0);
				myGLCD.setBackColor(230, 230, 230);
				myGLCD.print("IN", 30, 155);
				myGLCD.print("OUT", 142, 155);
			}
		}
		myGLCD.setColor(255, 255, 255);
		myGLCD.setBackColor(40, 104, 176);
		if (language == 1) myFiles.load(120, 198, 80, 20, "han/intro/enter.RAW");
		else myGLCD.print("ENTER", 120, 200);
		myGLCD.drawLine(190, 229, 230, 229);
//		Serial.println(rtc.getDayofWeek());
		page_num = intro_i;
		break;
	case intro_i:
		if ((intro_type == 1) || (intro_type == 4))
		{
			myGLCD.setFont(BigFont);
			myGLCD.setColor(30, 30, 30);
			myGLCD.setBackColor(255, 255, 255);
			if (rtc_month >= 10)
				myGLCD.print(String(rtc_month), 80, 25);
			else
			{
				myGLCD.setColor(255, 255, 255);
				myGLCD.fillRect(80, 25, 95, 40);
				myGLCD.setColor(0, 0, 0);
				myGLCD.print(String(rtc_month), 96, 25);
			}
			myGLCD.print(".", 112, 25);
			if (rtc_day >= 10)
				myGLCD.print(String(rtc_day), 128, 25);
			else
			{
				myGLCD.setColor(255, 255, 255);
				myGLCD.fillRect(128, 25, 143, 40);
				myGLCD.setColor(0, 0, 0);
				myGLCD.print(String(rtc_day), 144, 25);
			}
			myGLCD.print(daynames[rtc.getDayofWeek()], 185, 25);

			myGLCD.setFont(SevenSegNumFontPlusPlus);
			if (rtc_hour >= 10)
				myGLCD.print(String(rtc_hour), 80, 50);
			else
			{
				myGLCD.setColor(255, 255, 255);
				myGLCD.fillRect(80, 50, 111, 100);
				myGLCD.setColor(0, 0, 0);
				myGLCD.print(String(rtc_hour), 112, 50);
			}
			myGLCD.print(":", 148, 50);
			if (rtc_minute >= 10)
				myGLCD.print(String(rtc_minute), 180, 50);
			else
			{
				myGLCD.setColor(255, 255, 255);
				myGLCD.fillRect(180, 50, 212, 100);
				myGLCD.setColor(0, 0, 0);
				myGLCD.print(String(rtc_minute), 212, 50);
			}
		}
		float kva1;
		int kva2;
		myGLCD.setFont(BigFont);
		myGLCD.setColor(220, 0, 0);
		myGLCD.setBackColor(230, 230, 230);
		if ((hex_data[0]%10) != 0)
		{
			kva1 = (float)hex_data[0]/10;	
			if(hex_data[0]>99) {
				myGLCD.printNumF(kva1, 1, 18, 130);
			}
			else myGLCD.printNumF(kva1, 1, 34, 130);
		}
		else {
			kva2 = hex_data[0]/10;	
			myGLCD.print(String(kva2), 34, 130);
		}
		position2(1, 70, 155);
		position2(2, 214, 155);
		position2(3, 154, 130);
		break;
	case main:
		myGLCD.fillScr(255, 255, 255); // White Color
		myGLCD.setColor(0, 0, 0);
		//						myGLCD.fillRect (0, 0, 319, 209);
		/*****menu******/
		myGLCD.setColor(255, 255, 255);
		myGLCD.fillRect(1, 1, 100, 30);
		myGLCD.setColor(248, 111, 30);
		myGLCD.fillRect(1, 200, 79, 238);
		myGLCD.fillRect(81, 200, 159, 238);
		myGLCD.fillRect(161, 200, 239, 238);
		myGLCD.fillRect(241, 200, 318, 238);
		myGLCD.setColor(30, 30, 30);
		myGLCD.drawRect(1, 1, 100, 30);
		myGLCD.drawRect(1, 200, 79, 238);
		myGLCD.drawRect(81, 200, 159, 238);
		myGLCD.drawRect(161, 200, 239, 238);
		myGLCD.drawRect(241, 200, 318, 238);

		/*****line******/
		myGLCD.setColor(97, 97, 97); // dark gray
		myGLCD.drawLine(9, 99, 26, 99);
		myGLCD.drawLine(39, 99, 56, 99);
		myGLCD.drawLine(102, 99, 164, 99);
		myGLCD.drawLine(210, 99, 227, 99);
		myGLCD.drawLine(263, 99, 280, 99);
		myGLCD.drawLine(293, 99, 310, 99);
		myGLCD.drawLine(9, 112, 56, 112);
		myGLCD.drawLine(102, 112, 126, 112);
		myGLCD.drawLine(139, 112, 164, 112);
		myGLCD.drawLine(210, 112, 227, 112);
		myGLCD.drawLine(263, 112, 310, 112);
		myGLCD.drawLine(27, 46, 227, 46);
		myGLCD.drawLine(263, 46, 292, 46);
		myGLCD.drawLine(39, 59, 227, 59);
		myGLCD.drawLine(263, 59, 280, 59);
		myGLCD.drawLine(8, 99, 8, 112);
		myGLCD.drawLine(26, 46, 26, 98);
		myGLCD.drawLine(39, 60, 39, 98);
		myGLCD.drawLine(126, 112, 126, 137);
		myGLCD.drawLine(139, 113, 139, 137);
		myGLCD.drawLine(280, 60, 280, 98);
		myGLCD.drawLine(293, 46, 293, 98);
		myGLCD.drawLine(311, 99, 311, 112);
		/*****information******/

		if (language == 1)
		{
			myFiles.load(24, 212, 32, 16, "han/main/control_btn.RAW");
			myFiles.load(104, 212, 32, 16, "han/main/measure_btn.RAW");
			myFiles.load(184, 212, 32, 16, "han/main/setup_btn.RAW");
			myFiles.load(264, 212, 32, 16, "han/main/alarm_btn.RAW");
			myFiles.load(19, 8, 64, 16, "han/main/intro_btn.RAW");
		}
		else
		{
			myGLCD.setFont(Retro);
			myGLCD.setColor(0, 0, 0);
			myGLCD.setBackColor(255, 255, 255);
			myGLCD.print("INTRO", 30, 9);
			myGLCD.setBackColor(248, 111, 30);
			myGLCD.print("CONTROL", 12, 212);
			myGLCD.print("METER", 100, 212);
			myGLCD.print("SETUP", 180, 212);
			myGLCD.print("ALARM", 260, 212);
		}
		myGLCD.setColor(224, 211, 97);
		myGLCD.drawRoundRect(115, 138, 150, 181);

		myGLCD.drawRoundRect(57, 85, 101, 128);	 // converter
		myGLCD.drawRoundRect(165, 85, 209, 128); // inverter
		myGLCD.drawRoundRect(228, 40, 262, 66);	 // sts
		myGLCD.drawRoundRect(228, 93, 262, 119); // sts

		myGLCD.setColor(222, 237, 84);
		myGLCD.fillRoundRect(116, 139, 149, 180);
		myGLCD.setColor(40, 40, 40);
		myGLCD.drawLine(133, 141, 133, 149);
		myGLCD.drawLine(133, 170, 133, 178);
		myGLCD.drawLine(118, 150, 147, 150);
		myGLCD.drawLine(123, 155, 142, 155);
		myGLCD.drawLine(123, 156, 142, 156);
		myGLCD.drawLine(123, 157, 142, 157);
		myGLCD.drawLine(118, 162, 147, 162);
		myGLCD.drawLine(123, 167, 142, 167);
		myGLCD.drawLine(123, 168, 142, 168);
		myGLCD.drawLine(123, 169, 142, 169);
		myGLCD.setFont(BigFont);

		myGLCD.setColor(255, 0, 0);
		myGLCD.setBackColor(255, 255, 255);
		myGLCD.print("BAT", 181, 145);
		myGLCD.print("LOAD", 173, 170);
		myGLCD.print("%", 300, 145);
		myGLCD.print("%", 300, 170);
		myGLCD.setBackColor(22, 108, 171);
		myGLCD.setColor(255, 255, 255);
		page_num = main_next;
		break;
	case main_next:

		int main_alarm1, main_alarm2;
		int main_routine;
		main_alarm2 = hex_data[12] >> 7;
		main_check = main_alarm2 & 0x0001;
		if (main_check == 0)
		{
			//								myGLCD.setColor(203, 14, 14);		//soft red
			myGLCD.setColor(33, 184, 3);	   // green
			myGLCD.fillRect(9, 100, 26, 111);  // 1
			myGLCD.fillRect(27, 100, 38, 111); // in center
			myGLCD.fillRect(39, 100, 56, 111); // 3
			myGLCD.fillRect(27, 47, 38, 99);   // bypass 1
			myGLCD.fillRect(39, 47, 227, 58);  // bypass 2
		}
		else
		{
			//								myGLCD.setColor(90, 90, 90);		//green
			myGLCD.setColor(235, 235, 235);	   // light gray
			myGLCD.fillRect(9, 100, 26, 111);  // 1
			myGLCD.fillRect(27, 100, 38, 111); // in center
			myGLCD.fillRect(39, 100, 56, 111); // 3
			myGLCD.fillRect(27, 47, 38, 99);   // bypass 1
			myGLCD.fillRect(39, 47, 227, 58);  // bypass 2
		}
		main_alarm1 = hex_data[10];
		main_check = main_alarm1 & 0x0001;
		con_check = main_check; // converter on off check
		if (main_check == 0)
		{
			myGLCD.setColor(235, 235, 235);			// light gray
			myGLCD.fillRect(102, 100, 126, 111);	// dc 1
			myGLCD.fillRect(127, 100, 138, 111);	// dc center
			myGLCD.fillRect(139, 100, 164, 111);	// dc 2
			myGLCD.setColor(60, 60, 60);			// blue
			myGLCD.fillRoundRect(58, 86, 100, 127); // converter
			ac_onoff = 0;
			ac_model(66, 96);
		}
		else
		{
			//								myGLCD.setColor(203, 14, 14);		//soft red
			myGLCD.setColor(33, 184, 3);			// green
			myGLCD.fillRect(102, 100, 126, 111);	// dc 1
			myGLCD.fillRect(127, 100, 138, 111);	// dc center
			myGLCD.fillRect(139, 100, 164, 111);	// dc 2
			myGLCD.setColor(22, 108, 171);			// blue
			myGLCD.fillRoundRect(58, 86, 100, 127); // converter
			ac_onoff = 1;
			ac_model(66, 96);
		}
		main_alarm1 = hex_data[10] >> 1;
		main_check = main_alarm1 & 0x0001;
		if (main_check == 0)
		{
			myGLCD.setColor(235, 235, 235);		 // light gray
			myGLCD.fillRect(127, 112, 138, 137); // dc battery
			myGLCD.fillRect(127, 100, 138, 111); // dc center
			myGLCD.fillRect(139, 100, 164, 111); // dc 2
		}
		else
		{
			//								myGLCD.setColor(203, 14, 14);		//soft red
			myGLCD.setColor(33, 184, 3);		 // green
			myGLCD.fillRect(127, 112, 138, 137); // dc battery
			myGLCD.fillRect(127, 100, 138, 111); // dc center
			myGLCD.fillRect(139, 100, 164, 111); // dc 2
		}
		if ((main_check == 0) && (con_check == 1))
		{
			//								myGLCD.setColor(203, 14, 14);		//soft red
			myGLCD.setColor(33, 184, 3);		 // green
			myGLCD.fillRect(127, 100, 138, 111); // dc center
			myGLCD.fillRect(139, 100, 164, 111); // dc 2
		}
		main_alarm1 = hex_data[10] >> 2;
		main_check = main_alarm1 & 0x0001;
		if (main_check == 0)
		{
			myGLCD.setColor(235, 235, 235);			 // light gray
			myGLCD.fillRect(210, 100, 227, 111);	 // inv-sts
			myGLCD.setColor(60, 60, 60);			 // blue
			myGLCD.fillRoundRect(166, 86, 208, 127); // inverter
			ac_onoff = 0;
			ac_model(190, 117);
		}
		else
		{
			//							myGLCD.setColor(203, 14, 14);		//soft red
			myGLCD.setColor(33, 184, 3);			 // green
			myGLCD.fillRect(210, 100, 227, 111);	 // inv-sts
			myGLCD.setColor(22, 108, 171);			 // blue
			myGLCD.fillRoundRect(166, 86, 208, 127); // inverter
			ac_onoff = 1;
			ac_model(190, 117);
		}
		main_alarm1 = hex_data[10] >> 3;
		main_check = main_alarm1 & 0x0001;
		if (main_check == 0)
		{
			//								myGLCD.setColor(203, 14, 14);		//soft red
			myGLCD.setColor(33, 184, 3);		 // green
			myGLCD.fillRect(263, 47, 292, 58);	 // bypass 3
			myGLCD.fillRect(281, 59, 292, 99);	 // bypass 4
			myGLCD.fillRect(281, 100, 292, 111); // out center
			myGLCD.fillRect(293, 100, 310, 111); // out 2
			myGLCD.setColor(235, 235, 235);		 // light gray
			myGLCD.fillRect(263, 100, 280, 111); // out 1
			myGLCD.setColor(243, 155, 7);
			myGLCD.fillRoundRect(229, 41, 261, 65); // sts
			myGLCD.setColor(90, 90, 90);
			myGLCD.fillRoundRect(229, 94, 261, 118); // sts
		}
		else
		{
			//								myGLCD.setColor(203, 14, 14);		//soft red
			myGLCD.setColor(33, 184, 3);		 // green
			myGLCD.fillRect(263, 100, 280, 111); // out 1
			myGLCD.fillRect(281, 100, 292, 111); // out center
			myGLCD.fillRect(293, 100, 310, 111); // out 2
			myGLCD.setColor(235, 235, 235);		 // light gray
			myGLCD.fillRect(263, 47, 292, 58);	 // bypass 3
			myGLCD.fillRect(281, 59, 292, 99);	 // bypass 4
			myGLCD.setColor(90, 90, 90);
			myGLCD.fillRoundRect(229, 41, 261, 65); // sts
			myGLCD.setColor(243, 155, 7);
			myGLCD.fillRoundRect(229, 94, 261, 118); // sts
		}
		myGLCD.setColor(255, 255, 255);
		// converter
		myGLCD.drawLine(58, 127, 99, 86);
		myGLCD.drawLine(79, 113, 97, 113);
		myGLCD.drawLine(79, 114, 97, 114);
		myGLCD.drawLine(79, 119, 97, 119);
		myGLCD.drawLine(79, 120, 97, 120);
		// inverter
		myGLCD.drawLine(166, 127, 207, 86);
		myGLCD.drawLine(170, 92, 188, 92);
		myGLCD.drawLine(170, 93, 188, 93);
		myGLCD.drawLine(170, 98, 188, 98);
		myGLCD.drawLine(170, 99, 188, 99);

		myGLCD.setColor(0, 0, 0);
		// bypass scr
		myGLCD.drawLine(245, 43, 245, 63);
		myGLCD.drawLine(234, 45, 245, 53);
		myGLCD.drawLine(234, 61, 245, 53);
		myGLCD.drawLine(234, 45, 234, 61);
		myGLCD.drawLine(256, 45, 245, 53);
		myGLCD.drawLine(256, 61, 245, 53);
		myGLCD.drawLine(256, 45, 256, 61);
		myGLCD.drawLine(245, 53, 250, 63);
		myGLCD.drawLine(245, 53, 240, 43);
		// inverter scr
		myGLCD.drawLine(245, 97, 245, 116);
		myGLCD.drawLine(234, 98, 245, 106);
		myGLCD.drawLine(234, 114, 245, 106);
		myGLCD.drawLine(234, 98, 234, 114);
		myGLCD.drawLine(256, 98, 245, 106);
		myGLCD.drawLine(256, 114, 245, 106);
		myGLCD.drawLine(256, 98, 256, 114);
		myGLCD.drawLine(245, 106, 250, 116);
		myGLCD.drawLine(245, 106, 240, 96);
		main_system_check1 = hex_data[10];
		main_system_check2 = hex_data[12];
		buzzer_click = 0;
		//  					if (buzzer_switch==1) myFiles.load (30, 150, 50, 50, "background/speaker.RAW");
		page_num = main_i;
		break;
	case main_i:
		if ((buzzer_switch == 1) && (buzzer_click == 0))
		{
			myGLCD.drawBitmap(25, 135, 50, 50, speaker);
			//   							myFiles.load (25, 135, 50, 50, "background/speaker.RAW");
			buzzer_click = 1;
		}
		else if (buzzer_switch == 0)
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(25, 135, 75, 185);
		}
		myGLCD.setFont(Retro);
		myGLCD.setColor(0, 0, 0);
		myGLCD.setBackColor(255, 255, 255);
		myGLCD.print(String(rtc_year), 126, 5);
		myGLCD.print("/", 158, 5);
		if (rtc_month >= 10)
			myGLCD.print(String(rtc_month), 166, 5);
		else
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(166, 5, 173, 20);
			myGLCD.setColor(0, 0, 0);
			myGLCD.print(String(rtc_month), 174, 5);
		}
		myGLCD.print("/", 182, 5);
		if (rtc_day >= 10)
			myGLCD.print(String(rtc_day), 190, 5);
		else
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(190, 5, 197, 20);
			myGLCD.setColor(0, 0, 0);
			myGLCD.print(String(rtc_day), 198, 5);
		}
		myGLCD.print(daynames[rtc.getDayofWeek()], 216, 5);
		if (rtc_hour >= 10)
			myGLCD.print(String(rtc_hour), 250, 5);
		else
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(250, 5, 257, 20);
			myGLCD.setColor(0, 0, 0);
			myGLCD.print(String(rtc_hour), 258, 5);
		}
		myGLCD.print(":", 266, 5);
		if (rtc_minute >= 10)
			myGLCD.print(String(rtc_minute), 274, 5);
		else
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(274, 5, 281, 20);
			myGLCD.setColor(0, 0, 0);
			myGLCD.print(String(rtc_minute), 282, 5);
		}
		myGLCD.print(":", 290, 5);
		if (rtc_second >= 10)
			myGLCD.print(String(rtc_second), 298, 5);
		else
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(298, 5, 304, 20);
			myGLCD.setColor(0, 0, 0);
			myGLCD.print(String(rtc_second), 305, 5);
		}
		myGLCD.setFont(BigFont);
		position(6, 245, 145);
		position(7, 245, 170);
		if ((main_system_check1 != hex_data[10]) || (main_system_check2 != hex_data[12]))
			page_num = main_next;
		break;
	case sett:
		myGLCD.setFont(BigFont);
		myGLCD.fillScr(255, 255, 255);
		myGLCD.setColor(60, 60, 60);
		myGLCD.fillRect(0, 0, 319, 165);
		myGLCD.setColor(158, 158, 158);
		myGLCD.fillRect(5, 5, 314, 40);
		myGLCD.fillRect(5, 45, 314, 80);
		myGLCD.fillRect(5, 85, 314, 120);
		myGLCD.fillRect(5, 125, 314, 160);

		myGLCD.setColor(116, 175, 46);
		myGLCD.fillRect(80, 185, 190, 235);
		myGLCD.fillRect(200, 185, 310, 235);

		myGLCD.setColor(255, 255, 255);
		myGLCD.fillRect(220, 8, 302, 37);
		myGLCD.fillRect(220, 48, 302, 77);
		myGLCD.fillRect(220, 88, 302, 117);
		myGLCD.fillRect(220, 128, 302, 157);
		myGLCD.setColor(0, 0, 0);
		myGLCD.drawRect(80, 185, 190, 235);
		myGLCD.drawRect(200, 185, 310, 235);
		myGLCD.drawRect(220, 8, 302, 37);
		myGLCD.drawRect(220, 48, 302, 77);
		myGLCD.drawRect(220, 88, 302, 117);
		myGLCD.drawRect(220, 128, 302, 157);
		if (language == 1)
		{
			myGLCD.setColor(255, 255, 255);
			myFiles.load(20, 13, 162, 20, "han/setup/battery_v.RAW");
			myFiles.load(20, 53, 162, 20, "han/setup/battery_a.RAW");
			myFiles.load(20, 93, 142, 20, "han/setup/output_v.RAW");
			myFiles.load(20, 133, 184, 20, "han/setup/battery_u.RAW");

			myFiles.load(116, 201, 40, 20, "han/setup/time_s.RAW");
			myFiles.load(236, 201, 40, 20, "han/control/esc.RAW");
		}
		else
		{
			myGLCD.setColor(0, 0, 0);
			myGLCD.setBackColor(255, 255, 255);
			myGLCD.print("BATTERY [V]", 20, 15);
			myGLCD.print("BATTERY [A]", 20, 55);
			myGLCD.print("OUTPUT [V]", 20, 95);
			myGLCD.print("BAT_PIS [V]", 20, 135);
			myGLCD.setBackColor(116, 175, 46);
			myGLCD.setColor(255, 255, 255);
			myGLCD.print("ESC", 232, 203);
			myGLCD.print("TIME", 104, 203);
		}
		page_num = sett_i;
		break;
	case sett_i:
		position2(51, 240, 14);
		position2(50, 240, 54);
		position2(52, 240, 94);
		position2(53, 240, 134);
		break;
	case meter:
		myGLCD.fillScr(0, 0, 0);
		background_meter();
		page_num = meter_i;
		break;
	case meter_i:
		myGLCD.setColor(255, 255, 255);
		if (hex_data[39] != 0)
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(247, 53, 248, 54); // hz point
		}
		else
		{
			myGLCD.setColor(70, 70, 70);
			myGLCD.fillRect(247, 53, 248, 54); // hz point
		}
		if (hex_data[40] != 0)
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(247, 111, 248, 112); // hz point
		}
		else
		{
			myGLCD.setColor(70, 70, 70);
			myGLCD.fillRect(247, 111, 248, 112); // hz point
		}
		if (hex_data[41] != 0)
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(247, 169, 248, 170); // hz point
		}
		else
		{
			myGLCD.setColor(70, 70, 70);
			myGLCD.fillRect(247, 169, 248, 170); // hz point
		}
		position(30, 24, 37);
		position(31, 120, 37);
		position(39, 216, 37);
		position(35, 24, 95);
		position(36, 120, 95);
		position(40, 216, 95);
		position(37, 24, 153);
		position(38, 120, 153);
		position(41, 216, 153);
		position(33, 24, 211);
		position(34, 120, 211);

		break;
	case control:
		myGLCD.setFont(BigFont);
		myGLCD.fillScr(255, 255, 255);
		myGLCD.setColor(60, 60, 60);
		myGLCD.fillRect(0, 0, 319, 109);
		myGLCD.setColor(158, 158, 158);
		myGLCD.fillRect(5, 5, 314, 52);
		myGLCD.fillRect(5, 57, 314, 104);
		myGLCD.setColor(116, 175, 46);
		myGLCD.fillRect(200, 185, 310, 235);
		myGLCD.setColor(0, 0, 0);
		myGLCD.drawRect(200, 185, 310, 235);

		myGLCD.setBackColor(255, 255, 255);
		myGLCD.setColor(0, 0, 0);
		myGLCD.print(":", 83, 123);
		myGLCD.print(":", 83, 152);
		myGLCD.print(":", 83, 181);
		myGLCD.print(":", 83, 210);

		myGLCD.print("V", 147, 123);
		myGLCD.print("V", 147, 152);
		myGLCD.print("V", 147, 181);
		myGLCD.print("V", 147, 210);

		if (language == 1)
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.fillRect(33, 16, 186, 40);
			myGLCD.fillRect(33, 68, 186, 92);
			myFiles.load(18, 123, 64, 16, "han/measure/vdc_m.RAW");
			myFiles.load(18, 152, 64, 16, "han/measure/battery_m.RAW");
			myFiles.load(18, 181, 64, 16, "han/measure/inverter_m.RAW");
			myFiles.load(18, 210, 64, 16, "han/measure/output_m.RAW");
			myFiles.load(38, 19, 144, 19, "han/control/ups.RAW");
			myFiles.load(38, 71, 144, 19, "han/control/ups.RAW");
			myFiles.load(236, 201, 40, 20, "han/control/esc.RAW");
		}
		else
		{
			myGLCD.print("VDC", 35, 123);
			myGLCD.print("BAT", 35, 152);
			myGLCD.print("INV", 35, 181);
			myGLCD.print("OUT", 35, 210);

			myGLCD.setBackColor(255, 255, 255);
			myGLCD.print("UPS SYSTEM", 20, 21);
			myGLCD.print("UPS SYSTEM", 20, 73);

			myGLCD.setBackColor(116, 175, 46);
			myGLCD.setColor(255, 255, 255);
			myGLCD.print("ESC", 232, 203);
		}

		page_num = control_next;
		break;
	case control_next:
		control_alarm = hex_data[10]; // CONVERTER RUN/STOP
		control_check = (control_alarm >> 6) & 0x0001;
		if (control_check == 0)
		{
			myGLCD.setColor(208, 208, 208);
			myGLCD.fillRect(224, 7, 312, 50);
			myGLCD.setColor(216, 31, 16);
			myGLCD.fillRect(224, 59, 312, 102);
			if (language == 1)
			{
				myFiles.load(249, 19, 40, 20, "han/control/upson_off.RAW");
				myFiles.load(249, 71, 40, 20, "han/control/upsoff_on.RAW");
			}
			else
			{
				myGLCD.setColor(0, 0, 0);
				myGLCD.setBackColor(208, 208, 208);
				myGLCD.print("ON", 252, 21);
				myGLCD.setColor(255, 255, 255);
				myGLCD.setBackColor(216, 31, 16);
				myGLCD.print("OFF", 244, 73);
			}
		}
		else if (control_check == 1)
		{
			myGLCD.setColor(216, 31, 16);
			myGLCD.fillRect(224, 7, 312, 50);
			myGLCD.setColor(208, 208, 208);
			myGLCD.fillRect(224, 59, 312, 102);
			if (language == 1)
			{
				myFiles.load(249, 19, 40, 20, "han/control/upson_on.RAW");
				myFiles.load(249, 71, 40, 20, "han/control/upsoff_off.RAW");
			}
			else
			{
				myGLCD.setColor(255, 255, 255);
				myGLCD.setBackColor(216, 31, 16);
				myGLCD.print("ON", 252, 21);
				myGLCD.setColor(0, 0, 0);
				myGLCD.setBackColor(208, 208, 208);
				myGLCD.print("OFF", 244, 73);
			}
		}
		myGLCD.setColor(255, 255, 255);
		myGLCD.drawRect(224, 7, 312, 50);
		myGLCD.drawRect(224, 59, 312, 102);
		page_num = control_i;
		break;
	case control_i:
		myGLCD.setColor(255, 0, 0);
		position2(16, 99, 123);
		position2(17, 99, 151);
		position2(18, 99, 181);
		position2(19, 99, 210);
		control_alarm = hex_data[10];
		control_check = (control_alarm >> 6) & 0x0001;
		if (control_check != control_check_save)
			page_num = control_next;
		control_check_save = control_check;
		break;
	case alarm1:
		background_alarm();
		alarm_set();
		page_num = alarm1_i;
		break;
	case alarm1_i:
		alarm_set();
		myGLCD.setColor(255, 255, 255);
		myGLCD.setFont(BigFont);
		myGLCD.setBackColor(116, 175, 46);
		if (language == 1)
			myGLCD.print(String(alarm_number_i), 200, 5);
		else
			myGLCD.print(String(alarm_number_i), 235, 5);
		break;
	case alarm2:
		background_alarm();
		page_num = alarm2_i;
		break;
	case alarm2_i:
		alarm_set();
		myGLCD.setColor(255, 255, 255);
		myGLCD.setFont(BigFont);
		myGLCD.setBackColor(116, 175, 46);
		if (language == 1)
			myGLCD.print(String(alarm_number_i), 200, 5);
		else
			myGLCD.print(String(alarm_number_i), 235, 5);
		break;
	case alarm3:
		background_alarm();
		page_num = alarm3_i;
		break;
	case alarm3_i:
		alarm_set();
		myGLCD.setColor(255, 255, 255);
		myGLCD.setFont(BigFont);
		myGLCD.setBackColor(116, 175, 46);
		if (language == 1)
			myGLCD.print(String(alarm_number_i), 200, 5);
		else
			myGLCD.print(String(alarm_number_i), 235, 5);
		break;
	case history:
		background_history();
		eeprom_address_h = dueFlashStorage.read(4094);
		eeprom_address_l = dueFlashStorage.read(4095);
		eeprom_address = (eeprom_address_l & 0x00ff) | ((eeprom_address_h << 8) & 0xff00);
		z = eeprom_address;
		myGLCD.setColor(255, 255, 255);
		myGLCD.fillRect(0, 40, 319, 189);
		y11 = 12;
		eeprom_history();
		Serial.println(eeprom_address);
		page_num = history_i;
		break;
	case history_next:
		background_history();
		y11 = 12;
		myGLCD.setColor(255, 255, 255);
		myGLCD.fillRect(0, 40, 319, 189);
		eeprom_history();
		page_num = history_i;
		break;
	case history_i:
		break;
	case timeset:
		myGLCD.fillScr(255, 255, 255);
		myGLCD.setColor(50, 50, 50);
		myGLCD.fillRoundRect(20, 20, 104, 90);
		myGLCD.fillRoundRect(118, 20, 202, 90);
		myGLCD.fillRoundRect(216, 20, 300, 90);
		myGLCD.fillRoundRect(20, 104, 104, 174);
		myGLCD.fillRoundRect(118, 104, 202, 174);
		myGLCD.fillRoundRect(216, 104, 300, 174);
		myGLCD.setColor(0, 0, 0);
		myGLCD.drawRoundRect(20, 20, 104, 90);
		myGLCD.drawRoundRect(118, 20, 202, 90);
		myGLCD.drawRoundRect(216, 20, 300, 90);
		myGLCD.drawRoundRect(20, 104, 104, 174);
		myGLCD.drawRoundRect(118, 104, 202, 174);
		myGLCD.drawRoundRect(216, 104, 300, 174);
		myGLCD.setFont(BigFont);
		myGLCD.setColor(255, 255, 255);
		myGLCD.drawLine(25, 44, 99, 44);
		myGLCD.drawLine(123, 44, 197, 44);
		myGLCD.drawLine(221, 44, 295, 44);
		myGLCD.drawLine(25, 128, 99, 128);
		myGLCD.drawLine(123, 128, 197, 128);
		myGLCD.drawLine(221, 128, 295, 128);

		myGLCD.setColor(116, 175, 46);
		myGLCD.fillRect(200, 185, 310, 235);
		myGLCD.setColor(0, 0, 0);
		myGLCD.drawRect(200, 185, 310, 235);

		if (language == 1)
		{
			myFiles.load(53, 24, 18, 18, "han/time/year.RAW");
			myFiles.load(151, 24, 18, 18, "han/time/month.RAW");
			myFiles.load(249, 24, 18, 18, "han/time/day.RAW");
			myFiles.load(53, 108, 18, 18, "han/time/hour.RAW");
			myFiles.load(151, 108, 18, 18, "han/time/min.RAW");
			myFiles.load(249, 108, 18, 18, "han/time/second.RAW");
			myFiles.load(236, 201, 40, 20, "han/control/esc.RAW");
		}
		else
		{
			myGLCD.setColor(255, 255, 255);
			myGLCD.setBackColor(50, 50, 50);
			myGLCD.print("YEAR", 30, 25);
			myGLCD.print("MONTH", 120, 25);
			myGLCD.print("DAY", 234, 25);
			myGLCD.print("HOUR", 30, 109);
			myGLCD.print("MIN", 136, 109);
			myGLCD.print("SEC", 234, 109);
			myGLCD.setBackColor(116, 175, 46);
			myGLCD.print("ESC", 232, 203);
		}
		page_num = timeset_i;
		break;
	case timeset_i:
		time_segment_position((rtc_year - 2000), 38, 53);
		time_segment_position(rtc_month, 136, 53);
		time_segment_position(rtc_day, 234, 53);
		time_segment_position(rtc_hour, 38, 137);
		time_segment_position(rtc_minute, 136, 137);
		time_segment_position(rtc_second, 234, 137);

		break;
	case secret1:
		background_gain();
		if (language == 1)
		{
			myFiles.load(24, 212, 32, 16, "han/alarm/main_btn.RAW");
			myFiles.load(104, 212, 32, 16, "han/alarm/next_btn.RAW");
			myFiles.load(184, 212, 32, 16, "han/main/offset_btn.RAW");
			myFiles.load(264, 212, 32, 16, "han/alarm/esc_btn.RAW");
		}
		else
		{
			myGLCD.setFont(Retro);
			myGLCD.setColor(0, 0, 0);
			myGLCD.setBackColor(248, 111, 30);
			myGLCD.print("MAIN", 24, 212);
			myGLCD.print("NEXT", 104, 212);
			myGLCD.print("OFFSET", 176, 212);
			myGLCD.print("ESC", 268, 212);
		}

		myGLCD.setFont(BigFont);
		myGLCD.setColor(0, 0, 0);
		myGLCD.setBackColor(255, 255, 255);
		if (language == 1)
		{
			myFiles.load(25, 12, 64, 16, "han/measure/input_m.RAW");
			myFiles.load(25, 108, 64, 16, "han/measure/inverter_m.RAW");
		}
		else
		{
			myGLCD.print("INPUT", 20, 12);
			myGLCD.print("INVERTER", 20, 108);
		}
		myGLCD.setBackColor(60, 60, 60);
		myGLCD.setColor(255, 255, 255);
		myGLCD.print("V", 104, 36);
		myGLCD.print("A", 244, 36);
		myGLCD.print("V", 104, 132);
		myGLCD.print("A", 244, 132);
		page_num = secret1_i;
		break;
	case secret1_i:
		position(70, 56, 36);
		position(71, 196, 36);
		position(75, 56, 132);
		position(76, 196, 132);
		position2(60, 64, 65);
		position2(61, 212, 65);
		position2(65, 64, 161);
		position2(66, 212, 161);
		break;
	case secret2:
		background_gain();
		if (language == 1)
		{
			myFiles.load(24, 212, 32, 16, "han/alarm/main_btn.RAW");
			myFiles.load(104, 212, 32, 16, "han/alarm/prew_btn.RAW");
			myFiles.load(184, 212, 32, 16, "han/main/offset_btn.RAW");
			myFiles.load(264, 212, 32, 16, "han/alarm/esc_btn.RAW");
		}
		else
		{
			myGLCD.setFont(Retro);
			myGLCD.setColor(0, 0, 0);
			myGLCD.setBackColor(248, 111, 30);
			myGLCD.print("MAIN", 24, 212);
			myGLCD.print("PREW", 104, 212);
			myGLCD.print("OFFSET", 176, 212);
			myGLCD.print("ESC", 268, 212);
		}
		myGLCD.setFont(BigFont);
		myGLCD.setColor(0, 0, 0);
		myGLCD.setBackColor(255, 255, 255);
		if (language == 1)
		{
			myFiles.load(25, 12, 64, 16, "han/measure/battery_m.RAW");
			myFiles.load(25, 108, 64, 16, "han/measure/output_m.RAW");
			myFiles.load(180, 108, 64, 16, "han/measure/vdc_m.RAW");
		}
		else
		{
			myGLCD.print("BATTERY", 20, 12);
			myGLCD.print("OUTPUT    DC LINK", 20, 108);
		}
		myGLCD.setBackColor(60, 60, 60);
		myGLCD.setColor(255, 255, 255);
		myGLCD.print("V", 104, 36);
		myGLCD.print("A", 244, 36);
		myGLCD.print("A", 104, 132);
		myGLCD.print("V", 252, 132);
		page_num = secret2_i;
		break;
	case secret2_i:
		position(73, 56, 36);
		position(74, 196, 36);
		position(78, 56, 132);
		position(72, 204, 132);
		position2(63, 64, 65);
		position2(64, 212, 65);
		position2(68, 64, 161);
		position2(62, 212, 161);
		break;
	case offset:
		myGLCD.fillScr(255, 255, 255);
		myGLCD.setFont(BigFont);
		myGLCD.setBackColor(255, 255, 255);
		myGLCD.setColor(0, 0, 0);
		myGLCD.print("IN  V&A", 15, 12);
		myGLCD.print("VDC LINK", 15, 52);
		myGLCD.print("BAT V&A", 15, 92);
		myGLCD.print("INV V&A", 15, 132);
		myGLCD.print("OUT V&A", 15, 172);
		/*****menu******/
		myGLCD.setColor(248, 111, 30);
		myGLCD.fillRect(1, 200, 79, 238);
		myGLCD.fillRect(81, 200, 159, 238);
		myGLCD.fillRect(161, 200, 239, 238);
		myGLCD.fillRect(241, 200, 318, 238);
		myGLCD.setColor(30, 30, 30);
		myGLCD.drawRect(1, 200, 79, 238);
		myGLCD.drawRect(81, 200, 159, 238);
		myGLCD.drawRect(161, 200, 239, 238);
		myGLCD.drawRect(241, 200, 318, 238);
		if (language == 1)
		{
			myFiles.load(24, 212, 32, 16, "han/alarm/main_btn.RAW");
			myFiles.load(264, 212, 32, 16, "han/alarm/esc_btn.RAW");
		}
		else
		{
			myGLCD.setFont(Retro);
			myGLCD.setColor(0, 0, 0);
			myGLCD.setBackColor(248, 111, 30);
			myGLCD.print("MAIN", 24, 212);
			myGLCD.print("ESC", 268, 212);
		}
		page_num = offset_i;
		break;
	case offset_i:
		myGLCD.setColor(255, 0, 0);
		myGLCD.setFont(BigFont);
		position3(90, 147, 12);
		position3(91, 238, 12);
		position3(92, 238, 52);
		position3(93, 147, 92);
		position3(94, 238, 92);
		position3(95, 147, 132);
		position3(96, 238, 132);
		position3(97, 147, 172);
		position3(98, 238, 172);

		break;
	case keypad:
		drawButtons();
		page_num = keypad_i;
		break;
	case keypad_i:
		break;
	case time_set:
		if (keypad_set == year_keypad)
		{
			if (input_set_data > 99)
				input_set_data = 99;
			rtc.setYear(input_set_data);
		}
		if (keypad_set == month_keypad)
		{
			if (input_set_data > 12)
				input_set_data = 12;
			rtc.setMonth(input_set_data);
		}
		if (keypad_set == day_keypad)
		{
			if (input_set_data > 31)
				input_set_data = 31;
			rtc.setDay(input_set_data);
		}
		if (keypad_set == hour_keypad)
		{
			if (input_set_data > 23)
				input_set_data = 23;
			rtc.setHours(input_set_data);
		}
		if (keypad_set == minute_keypad)
		{
			if (input_set_data > 59)
				input_set_data = 59;
			rtc.setMinutes(input_set_data);
		}
		if (keypad_set == second_keypad)
		{
			if (input_set_data > 59)
				input_set_data = 59;
			rtc.setSeconds(input_set_data);
		}
		page_num = timeset;
		break;
	case black:
		myGLCD.fillScr(0, 0, 0);
		page_num = black_i;
		break;
	case black_i:
		break;
	}
}

