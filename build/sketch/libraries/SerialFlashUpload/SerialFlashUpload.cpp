#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\SerialFlashUpload\\SerialFlashUpload.cpp"
#include "SerialFlashUpload.h"
#include <FlashImagePack.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

extern "C" void SerialFlashUpload_kick(void);

Stream *SerialFlashUpload::_dbg = NULL;

void SerialFlashUpload::dbgLine(const char *msg) {
  if (_dbg) {
    _dbg->println(msg);
    _dbg->flush();
  }
}

uint32_t SerialFlashUpload::crc32(const uint8_t *data, uint32_t len, uint32_t crc) {
  crc = ~crc;
  while (len--) {
    crc ^= *data++;
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 1UL) {
        crc = (crc >> 1) ^ 0xEDB88320UL;
      } else {
        crc = crc >> 1;
      }
    }
  }
  return ~crc;
}

bool SerialFlashUpload::readLine(Stream &s, char *buf, uint8_t bufLen, uint32_t timeoutMs) {
  uint8_t n = 0;
  uint32_t start = millis();
  if (bufLen < 2) {
    return false;
  }
  while ((millis() - start) < timeoutMs) {
    SerialFlashUpload_kick();
    if (!s.available()) {
      continue;
    }
    char c = (char)s.read();
    if (c == '@') {
      /* COM5 가 0x20 을 0x40 으로 뒤집는 경우가 있다. */
      c = ' ';
    }
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      buf[n] = 0;
      return n > 0;
    }
    if (n < (uint8_t)(bufLen - 1)) {
      buf[n++] = c;
    }
  }
  buf[n] = 0;
  return false;
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

bool SerialFlashUpload::parseHexChunk(const char *line, uint32_t *offOut, uint8_t *chunk, uint16_t *nOut, uint32_t *crcOut) {
  if (strncmp(line, "C1DL D ", 7) != 0) {
    return false;
  }
  const char *p = line + 7;
  unsigned long off = 0;
  if (sscanf(p, "%lu", &off) != 1) {
    return false;
  }
  while (*p && *p != ' ') {
    p++;
  }
  if (*p != ' ') {
    return false;
  }
  p++;
  uint16_t n = 0;
  while (p[0] && p[1] && n < CHUNK_MAX) {
    if (p[0] == ' ') {
      break;
    }
    int hi = hexNibble(p[0]);
    int lo = hexNibble(p[1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    chunk[n++] = (uint8_t)((hi << 4) | lo);
    p += 2;
  }
  if (n == 0 || *p != ' ') {
    return false;
  }
  unsigned long crc = 0;
  if (sscanf(p + 1, "%lu", &crc) != 1) {
    return false;
  }
  *offOut = (uint32_t)off;
  *nOut = n;
  *crcOut = (uint32_t)crc;
  return true;
}

static void drainRx(Stream &s) {
  uint32_t start = millis();
  while ((millis() - start) < 20UL) {
    SerialFlashUpload_kick();
    if (s.available() > 0) {
      s.read();
      start = millis();
    }
  }
}

static void settleAfterFlash(Stream &s) {
  delay(15);
  drainRx(s);
}

static void sendChunkOk(Stream &s, uint32_t written) {
  s.print("C1DL A ");
  s.println(written);
  s.flush();
}

static void sendChunkNak(Stream &s, const char *why, uint32_t got, uint32_t want) {
  s.print("C1DL N ");
  s.print(why);
  s.print(" ");
  s.print(got);
  s.print(" ");
  s.println(want);
  s.flush();
}

static bool readExact(Stream &s, uint8_t *buf, uint32_t len, uint32_t idleMs) {
  uint32_t got = 0;
  uint32_t start = millis();
  while (got < len) {
    SerialFlashUpload_kick();
    if (s.available() > 0) {
      buf[got++] = (uint8_t)s.read();
      start = millis();
      continue;
    }
    if ((millis() - start) >= idleMs) {
      return false;
    }
  }
  return true;
}

static bool pathLooksValid(const char *path) {
  if (path[0] == 0) {
    return false;
  }
  uint8_t n = 0;
  bool slash = false;
  while (path[n]) {
    char c = path[n];
    if (c == '/') {
      slash = true;
    } else if (!(isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-')) {
      return false;
    }
    n++;
    if (n >= FlashImagePack::PATH_SIZE) {
      break;
    }
  }
  return slash && n >= 5;
}

bool SerialFlashUpload::receivePack(Stream &s, GD25Q80 &flash, uint32_t size, uint32_t expectCrc) {
  s.println("C1DL BEGINACK");
  s.flush();
  if (size == 0 || size > GD25Q80::CAPACITY) {
    s.println("C1DL FAIL");
    dbgLine("C1DL FAIL SIZE");
    return false;
  }

  s.println("C1DL ERASE");
  s.flush();
  dbgLine("C1DL ERASE");
  if (!flash.eraseChip(&s)) {
    s.println("C1DL FAIL ERASE");
    s.flush();
    dbgLine("C1DL FAIL ERASE");
    return false;
  }
  s.println("C1DL GO");
  s.flush();
  dbgLine("C1DL GO");
  delay(20);
  drainRx(s);

  uint32_t written = 0;
  uint32_t runCrc = 0;
  uint8_t pageBuf[GD25Q80::PAGE_SIZE];
  uint32_t lastPing = millis();

  while (written < size) {
    uint32_t remain = size - written;
    uint16_t n = (remain > GD25Q80::PAGE_SIZE) ? GD25Q80::PAGE_SIZE : (uint16_t)remain;
    if (!readExact(s, pageBuf, n, 2000)) {
      s.println("C1DL FAIL TO");
      s.flush();
      dbgLine("C1DL FAIL TO");
      return false;
    }
    runCrc = crc32(pageBuf, n, runCrc);
    if (!flash.write(written, pageBuf, n)) {
      s.println("C1DL FAIL WR");
      s.flush();
      dbgLine("C1DL FAIL WR");
      return false;
    }
    written += n;
    if ((millis() - lastPing) >= 400UL) {
      lastPing = millis();
      s.print("C1DL BUSY ");
      s.println(written);
      s.flush();
    }
  }

  if (runCrc != expectCrc) {
    s.println("C1DL FAIL CRC");
    s.flush();
    dbgLine("C1DL FAIL CRC");
    return false;
  }
  s.println("C1DL DONE");
  s.flush();
  dbgLine("C1DL DONE");
  return true;
}

bool SerialFlashUpload::writeFileInPlace(GD25Q80 &flash, uint32_t offset, const uint8_t *data, uint32_t size) {
  static uint8_t sector[GD25Q80::SECTOR_SIZE];
  uint32_t end = offset + size;
  uint32_t sec = offset & ~(GD25Q80::SECTOR_SIZE - 1UL);
  while (sec < end) {
    if (!flash.read(sec, sector, GD25Q80::SECTOR_SIZE)) {
      return false;
    }
    uint32_t copyStart = (offset > sec) ? offset : sec;
    uint32_t copyEnd = (end < (sec + GD25Q80::SECTOR_SIZE)) ? end : (sec + GD25Q80::SECTOR_SIZE);
    memcpy(sector + (copyStart - sec), data + (copyStart - offset), copyEnd - copyStart);
    if (!flash.eraseSector(sec)) {
      return false;
    }
    if (!flash.write(sec, sector, GD25Q80::SECTOR_SIZE)) {
      return false;
    }
    sec += GD25Q80::SECTOR_SIZE;
  }
  return true;
}

bool SerialFlashUpload::receivePut(Stream &s, GD25Q80 &flash, const char *path, uint32_t size, uint32_t expectCrc) {
  s.println("C1DL PUTACK");
  s.flush();
  FlashImagePack pack;
  if (!pack.begin(flash)) {
    s.println("C1DL FAIL PACK");
    s.flush();
    dbgLine("C1DL FAIL PACK");
    return false;
  }
  uint32_t off = 0;
  uint32_t oldSz = 0;
  if (!pack.find(path, &off, &oldSz)) {
    s.println("C1DL FAIL NOTFOUND");
    s.flush();
    dbgLine("C1DL FAIL NOTFOUND");
    return false;
  }
  if (size == 0 || size != oldSz || size > PUT_MAX) {
    s.println("C1DL FAIL SIZE");
    s.flush();
    dbgLine("C1DL FAIL SIZE");
    return false;
  }

  s.println("C1DL PUTGO");
  s.flush();
  dbgLine("C1DL PUTGO");
  delay(20);
  drainRx(s);

  static uint8_t fileBuf[PUT_MAX];
  if (!readExact(s, fileBuf, size, 2000)) {
    s.println("C1DL FAIL TO");
    s.flush();
    dbgLine("C1DL FAIL TO");
    return false;
  }
  uint32_t runCrc = crc32(fileBuf, size, 0);

  if (runCrc != expectCrc) {
    s.println("C1DL FAIL CRC");
    s.flush();
    dbgLine("C1DL FAIL CRC");
    return false;
  }
  if (!writeFileInPlace(flash, off, fileBuf, size)) {
    s.println("C1DL FAIL WR");
    s.flush();
    dbgLine("C1DL FAIL WR");
    return false;
  }
  s.println("C1DL PUTOK");
  s.flush();
  dbgLine("C1DL PUTOK");
  return true;
}

void SerialFlashUpload::sendList(Stream &s, GD25Q80 &flash) {
  FlashImagePack pack;
  if (!pack.begin(flash)) {
    s.println("C1DL LIST 0");
    s.println("C1DL LIST END");
    return;
  }
  char path[FlashImagePack::PATH_SIZE + 1];
  uint16_t shown = 0;
  for (uint16_t i = 0; i < pack.fileCount(); i++) {
    uint32_t off = 0;
    uint32_t sz = 0;
    if (!pack.getEntry(i, path, sizeof(path), &off, &sz)) {
      continue;
    }
    if (!pathLooksValid(path) || sz == 0 || sz > GD25Q80::CAPACITY) {
      continue;
    }
    shown++;
  }
  s.print("C1DL LIST ");
  s.println(shown);
  for (uint16_t i = 0; i < pack.fileCount(); i++) {
    uint32_t off = 0;
    uint32_t sz = 0;
    if (!pack.getEntry(i, path, sizeof(path), &off, &sz)) {
      continue;
    }
    if (!pathLooksValid(path) || sz == 0 || sz > GD25Q80::CAPACITY) {
      continue;
    }
    s.print("C1DL FILE ");
    s.print(path);
    s.print(" ");
    s.println(sz);
  }
  s.println("C1DL LIST END");
}

void SerialFlashUpload::session(Stream &s, GD25Q80 &flash) {
  char line[96];
  dbgLine("C1DL MODE");
  drainRx(s);
  while (true) {
    if (!s.available()) {
      continue;
    }
    if (!readLine(s, line, sizeof(line), 5000)) {
      continue;
    }
    uint8_t ln = (uint8_t)strlen(line);
    while (ln > 0 && (line[ln - 1] == ' ' || line[ln - 1] == '\t')) {
      line[--ln] = 0;
    }
    if (strcmp(line, "C1DL HELLO") == 0) {
      s.println("C1DL MODE");
      s.flush();
    } else if (strcmp(line, "C1DL LIST") == 0) {
      s.println("C1DL LISTACK");
      s.flush();
      sendList(s, flash);
    } else if (strcmp(line, "C1DL QUIT") == 0) {
      s.println("C1DL BYE");
      s.flush();
      dbgLine("C1DL BYE");
      return;
    } else if (strncmp(line, "C1DL BEGIN ", 11) == 0) {
      unsigned long size = 0;
      unsigned long crc = 0;
      if (sscanf(line + 11, "%lu %lu", &size, &crc) == 2) {
        receivePack(s, flash, (uint32_t)size, (uint32_t)crc);
      } else {
        s.println("C1DL FAIL");
        s.flush();
      }
    } else if (strncmp(line, "C1DL PUT ", 9) == 0) {
      char path[FlashImagePack::PATH_SIZE + 1];
      unsigned long size = 0;
      unsigned long crc = 0;
      if (sscanf(line + 9, "%40s %lu %lu", path, &size, &crc) == 3) {
        receivePut(s, flash, path, (uint32_t)size, (uint32_t)crc);
      } else {
        s.println("C1DL FAIL");
        s.flush();
      }
    } else {
      s.print("C1DL UNK ");
      s.println(line);
      s.flush();
    }
  }
}

bool SerialFlashUpload::poll(Stream &s, GD25Q80 &flash, uint32_t readyWindowMs, Stream *dbg) {
  _dbg = dbg;
  uint32_t start = millis();
  uint32_t lastHello = 0;
  char line[96];
  dbgLine("upload wait Serial1 115200");

  while (true) {
    if ((millis() - lastHello) >= 250UL) {
      s.println("C1DL HELLO");
      s.flush();
      lastHello = millis();
    }

    if (s.available()) {
      if (readLine(s, line, sizeof(line), 200)) {
        if (strcmp(line, "C1DL HELLO") == 0 || strcmp(line, "C1DL ACK") == 0) {
          s.println("C1DL MODE");
          s.flush();
          session(s, flash);
          return true;
        }
      }
    }

    if (readyWindowMs > 0 && (millis() - start) >= readyWindowMs) {
      s.println("C1DL BOOT");
      s.flush();
      dbgLine("C1DL BOOT");
      return false;
    }
  }
}
