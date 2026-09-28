#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\GD25Q80\\GD25Q80.h"
#ifndef GD25Q80_H
#define GD25Q80_H

#include <Arduino.h>
#include <SPI.h>

/*
 * 보드 SOP-8: Adesto/Renesas AT25SF081 (8Mbit / 1MB)
 * 명령 체계가 같아서 GD25Q80ETIGR 도 같은 드라이버로 인식한다.
 *
 * SD 카드와 SPI(MOSI/MISO/SCK) 및 CS(PB23 = Arduino 핀 78)를 병렬 공유.
 * CS가 같으면 SD와 플래시를 동시에 쓰면 MISO 충돌이 난다.
 *
 * JEDEC (CMD 0x9F)
 *   AT25SF081 : 0x1F 0x85 0x01
 *   GD25Q80   : 0xC8 0x40 0x14
 */
class GD25Q80 {
public:
  static const uint8_t  CMD_WRITE_ENABLE   = 0x06;
  static const uint8_t  CMD_WRITE_DISABLE  = 0x04;
  static const uint8_t  CMD_READ_STATUS    = 0x05;
  static const uint8_t  CMD_READ           = 0x03;
  static const uint8_t  CMD_PAGE_PROGRAM   = 0x02;
  static const uint8_t  CMD_SECTOR_ERASE   = 0x20;
  static const uint8_t  CMD_CHIP_ERASE     = 0xC7;
  static const uint8_t  CMD_JEDEC_ID       = 0x9F;
  static const uint8_t  CMD_RELEASE_DPD    = 0xAB;

  static const uint32_t JEDEC_AT25SF081    = 0x001F8501UL;
  static const uint32_t JEDEC_GD25Q80      = 0x00C84014UL;
  static const uint32_t CAPACITY           = 0x100000UL;  /* 1MB */
  static const uint16_t PAGE_SIZE          = 256;
  static const uint32_t SECTOR_SIZE        = 4096;

  explicit GD25Q80(uint8_t csPin = 78);

  bool begin(uint32_t spiHz = 4000000UL);
  bool probe();
  void dumpInfo(Stream &out);

  bool isPresent() const { return _present; }
  uint32_t jedecId() const { return _jedec; }
  const char *chipName() const;
  uint32_t capacity() const { return CAPACITY; }

  uint8_t readStatus();
  bool waitBusy(uint32_t timeoutMs = 5000);
  bool read(uint32_t addr, uint8_t *buf, uint32_t len);
  bool writeEnable();
  bool write(uint32_t addr, const uint8_t *buf, uint32_t len);
  bool eraseSector(uint32_t addr);
  bool eraseChip(Stream *progress = NULL);
  bool pageProgram(uint32_t addr, const uint8_t *buf, uint16_t len);

private:
  uint8_t _cs;
  uint32_t _jedec;
  bool _present;
  SPISettings _settings;

  void select();
  void deselect();
  void transferAddr(uint32_t addr);
};

#endif
