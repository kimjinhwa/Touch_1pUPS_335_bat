#ifndef SERIAL_FLASH_UPLOAD_H
#define SERIAL_FLASH_UPLOAD_H

#include <Arduino.h>
#include <GD25Q80.h>

/*
 * 이 스케치: Serial 115200 (PC COM5). 업로드 중에는 같은 포트에 로그/모니터 금지.
 * 명령은 ASCII 한 줄, 데이터는 길이만큼 바이너리.
 *   C1DL BEGIN <size> <crc32>  → GO 후 size 바이트 → DONE / FAIL CRC
 *   C1DL PUT <path> <size> <crc32> → PUTGO 후 size 바이트 → PUTOK
 * readyWindowMs==0 이면 HELLO 대기에서 빠져나가지 않는다.
 *
 * 부팅:
 *   단말 "C1DL HELLO" 를 약 2.5초 반복
 *   PC   "C1DL HELLO"  → 업로드 모드  (PC는 미리 대기)
 *   응답 없으면 "C1DL BOOT" 후 바로 정상 부팅
 */
class SerialFlashUpload {
public:
  static const uint16_t CHUNK_MAX = 16;
  static const uint32_t PUT_MAX = 32768;

  static bool poll(Stream &s, GD25Q80 &flash, uint32_t readyWindowMs = 2500, Stream *dbg = NULL);

private:
  static Stream *_dbg;
  static void dbgLine(const char *msg);
  static uint32_t crc32(const uint8_t *data, uint32_t len, uint32_t crc = 0);
  static bool readLine(Stream &s, char *buf, uint8_t bufLen, uint32_t timeoutMs);
  static bool parseHexChunk(const char *line, uint32_t *offOut, uint8_t *chunk, uint16_t *nOut, uint32_t *crcOut);
  static bool receivePack(Stream &s, GD25Q80 &flash, uint32_t size, uint32_t expectCrc);
  static bool receivePut(Stream &s, GD25Q80 &flash, const char *path, uint32_t size, uint32_t expectCrc);
  static bool writeFileInPlace(GD25Q80 &flash, uint32_t offset, const uint8_t *data, uint32_t size);
  static void sendList(Stream &s, GD25Q80 &flash);
  static void session(Stream &s, GD25Q80 &flash);
};

#endif
