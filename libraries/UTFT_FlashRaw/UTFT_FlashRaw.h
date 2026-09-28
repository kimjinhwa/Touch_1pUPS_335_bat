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
