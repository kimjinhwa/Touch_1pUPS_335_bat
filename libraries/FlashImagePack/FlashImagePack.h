#ifndef FLASH_IMAGE_PACK_H
#define FLASH_IMAGE_PACK_H

#include <Arduino.h>
#include <GD25Q80.h>

/*
 * 플래시 이미지 팩 (주소 0부터). SD 경로와 동일한 문자열로 찾는다.
 * 예: "img/menu/control.RAW"
 *
 * [헤더 16바이트]
 *   magic[4]     "C1IM"
 *   version      u16 LE = 1
 *   fileCount    u16 LE
 *   dataOffset   u32 LE
 *   crc32        u32 LE  (팩 전체, crc 필드는 0으로 두고 계산)
 * [엔트리 48바이트 * fileCount]
 *   path[40]     NUL 패딩, '/' 구분, ".RAW"
 *   offset       u32 LE  (플래시 절대 주소)
 *   size         u32 LE
 * [패딩] dataOffset까지 0xFF
 * [RAW 데이터...] 헤더 없는 RGB565 big-endian, UTFT_SdRaw 과 동일
 */
class FlashImagePack {
public:
  static const uint32_t MAGIC           = 0x4D493143UL; /* 'C1IM' LE */
  static const uint16_t VERSION         = 1;
  static const uint16_t HEADER_SIZE     = 16;
  static const uint16_t ENTRY_SIZE      = 48;
  static const uint16_t PATH_SIZE       = 40;

  FlashImagePack();

  bool begin(GD25Q80 &flash);
  bool headerOk() const { return _ok; }
  uint16_t fileCount() const { return _fileCount; }

  static bool headerOk(GD25Q80 &flash);

  bool find(const char *path, uint32_t *offset, uint32_t *size);
  bool getEntry(uint16_t index, char *pathOut, uint8_t pathOutLen, uint32_t *offset, uint32_t *size);
  bool read(uint32_t addr, uint8_t *buf, uint32_t len);

private:
  GD25Q80 *_flash;
  bool _ok;
  uint16_t _fileCount;
  uint32_t _dataOffset;

  static void pathNorm(const char *src, char *dst, uint8_t dstLen);
  static bool pathEqual(const char *a, const char *b);
};

#endif
