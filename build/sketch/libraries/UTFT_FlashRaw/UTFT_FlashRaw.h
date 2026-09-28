#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT_FlashRaw\\UTFT_FlashRaw.h"
#ifndef UTFT_FLASH_RAW_H
#define UTFT_FLASH_RAW_H

#include <UTFT.h>
#include <FlashImagePack.h>

class UTFT_FlashRaw {
public:
  UTFT_FlashRaw(UTFT *ptrUTFT, FlashImagePack *pack);

  int load(int x, int y, int sx, int sy, char *filename, int bufmult = 1, bool iswap = 0);

private:
  UTFT *_UTFT;
  FlashImagePack *_pack;
};

#endif
