#include "Storage.h"

extern UTFT myGLCD;
extern uint8_t intro_type;
extern uint8_t BigFont[];
extern uint8_t SmallFont[];
extern void WatchDogClear();

#ifdef USE_SPI_FLASH
extern "C" void SerialFlashUpload_kick(void) {
	WatchDogClear();
}
#endif

static const uint8_t kUnusedFlashCs = 87;

static void holdUnusedCs() {
	pinMode(kUnusedFlashCs, OUTPUT);
	digitalWrite(kUnusedFlashCs, HIGH);
}

#ifdef USE_SPI_FLASH

extern GD25Q80 extFlash;
extern FlashImagePack imagePack;

void storageBegin() {
	holdUnusedCs();
	if (!extFlash.begin()) {
		Serial.println("SPI flash not found");
		extFlash.dumpInfo(Serial);
		while (1) {
			WatchDogClear();
		}
	}
	/* HELLO 는 Serial(COM5) 로만. 같은 포트에 로그를 섞지 않는다. */
	SerialFlashUpload::poll(Serial, extFlash, 2500, NULL);
	extFlash.dumpInfo(Serial);
	if (imagePack.begin(extFlash)) {
		Serial.print("image pack files=");
		Serial.println(imagePack.fileCount());
	} else {
		Serial.println("image pack missing");
	}
}

void storageEnterUploadMode() {
	myGLCD.fillScr(0, 0, 0);
	myGLCD.setColor(255, 255, 255);
	myGLCD.setBackColor(0, 0, 0);
	myGLCD.setFont(BigFont);
	myGLCD.print("IMAGE UPLOAD", 40, 100);
	myGLCD.setFont(SmallFont);
	myGLCD.print("Serial 115200 COM5", 40, 140);
	Serial.flush();
	SerialFlashUpload::poll(Serial, extFlash, 0, NULL);
	if (imagePack.begin(extFlash)) {
		Serial.print("image pack files=");
		Serial.println(imagePack.fileCount());
	}
}

#else

extern SdFat sd;

#ifndef SD_CHIP_SELECT
#define SD_CHIP_SELECT 78
#endif

void storageBegin() {
	holdUnusedCs();
	if ((intro_type == 1) || (intro_type == 3)) {
		if (!sd.begin(SD_CHIP_SELECT, SPI_HALF_SPEED)) {
			Serial.println("SD HALF fail, retry QUARTER");
			if (!sd.begin(SD_CHIP_SELECT, SPI_QUARTER_SPEED)) {
				Serial.println("SD begin fail");
			}
		}
	}
}

#endif
