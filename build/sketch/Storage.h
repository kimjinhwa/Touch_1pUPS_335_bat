#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\Storage.h"
#ifndef TOUCH_UPS_STORAGE_H
#define TOUCH_UPS_STORAGE_H

/*
 * SD 카드와 AT25SF081 이미지 팩을 컴파일 옵션으로 가른다.
 * UI 쪽 myFiles.load("han/....RAW") 경로는 그대로 둔다.
 *
 *   기본 (SD): 옵션 없음
 *   플래시:    -DUSE_SPI_FLASH   (compile.ps1 -Flavor flash)
 */

#include <UTFT.h>

#ifdef USE_SPI_FLASH
#include <UTFT_FlashRaw.h>
#include <GD25Q80.h>
#include <FlashImagePack.h>
#include <SerialFlashUpload.h>

#define FLASH_CHIP_SELECT 78

extern GD25Q80 extFlash;
extern FlashImagePack imagePack;
extern UTFT_FlashRaw myFiles;

void storageEnterUploadMode();
#else
#include <SdFat.h>
#include <UTFT_SdRaw.h>

extern SdFat sd;
extern UTFT_SdRaw myFiles;
#endif

void storageBegin();

#endif
