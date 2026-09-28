#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\UTFT_FlashRaw\\UTFT_FlashRaw.cpp"
#include "UTFT_FlashRaw.h"

UTFT_FlashRaw::UTFT_FlashRaw(UTFT *ptrUTFT, FlashImagePack *pack) {
  _UTFT = ptrUTFT;
  _pack = pack;
}

int UTFT_FlashRaw::load(int x, int y, int sx, int sy, char *filename, int bufmult, bool iswap) {
  uint32_t addr = 0;
  uint32_t fileSize = 0;
  if (_pack == NULL || !_pack->find(filename, &addr, &fileSize)) {
    return 99;
  }

  char buf[2 * bufmult * sx];
  int cx, cy, cp;
  word result;
  uint32_t remain = fileSize;

  cbi(_UTFT->P_CS, _UTFT->B_CS);
  cx = 0;
  cy = 0;
  result = bufmult * sx;
  if (_UTFT->orient == PORTRAIT) {
    _UTFT->setXY(x, y, x + sx - 1, y + sy - 1);
  }
  for (int n = 0; n < sy; n += bufmult) {
    uint32_t want = (uint32_t)((2 * bufmult) * sx);
    if (want > remain) {
      want = remain;
    }
    if (want == 0 || !_pack->read(addr, (uint8_t *)buf, want)) {
      sbi(_UTFT->P_CS, _UTFT->B_CS);
      return 99;
    }
    addr += want;
    remain -= want;
    result = (word)want;
    if (_UTFT->orient == PORTRAIT) {
      for (int i = 0; i < result; i += 2) {
        if (iswap == 1) {
          _UTFT->LCD_Write_DATA(buf[i + 1], buf[i]);
        } else {
          _UTFT->LCD_Write_DATA(buf[i], buf[i + 1]);
        }
      }
    } else {
      cp = 0;
      while (cp < result) {
        if (((result - cp) / 2) < (sx - cx)) {
          _UTFT->setXY(x + cx, y + cy, x + cx + ((result - cp) / 2) - 1, y + cy);
          for (int i = (result - cp) - 2; i >= 0; i -= 2) {
            if (iswap == 1) {
              _UTFT->LCD_Write_DATA(buf[cp + i + 1], buf[cp + i]);
            } else {
              _UTFT->LCD_Write_DATA(buf[cp + i], buf[cp + i + 1]);
            }
          }
          cx += ((result - cp) / 2);
          cp = result;
        } else {
          _UTFT->setXY(x + cx, y + cy, x + sx - 1, y + cy);
          for (int i = sx - cx - 1; i >= 0; i--) {
            if (iswap) {
              _UTFT->LCD_Write_DATA(buf[cp + (i * 2) + 1], buf[cp + (i * 2)]);
            } else {
              _UTFT->LCD_Write_DATA(buf[cp + (i * 2)], buf[cp + (i * 2) + 1]);
            }
          }
          cp += (sx - cx) * 2;
          cx = 0;
          cy++;
        }
      }
    }
  }
  _UTFT->setXY(0, 0, _UTFT->getDisplayXSize() - 1, _UTFT->getDisplayYSize() - 1);
  sbi(_UTFT->P_CS, _UTFT->B_CS);
  return 0;
}
