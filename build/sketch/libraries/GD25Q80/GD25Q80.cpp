#line 1 "C:\\DevWork\\4.IFTechWork\\1.p1pDisplay3Inch\\Touch_1pUPS_335_bat\\libraries\\GD25Q80\\GD25Q80.cpp"
#include "GD25Q80.h"

extern "C" void SerialFlashUpload_kick(void);

GD25Q80::GD25Q80(uint8_t csPin)
  : _cs(csPin),
    _jedec(0),
    _present(false),
    _settings(4000000UL, MSBFIRST, SPI_MODE0) {
}

bool GD25Q80::begin(uint32_t spiHz) {
  _settings = SPISettings(spiHz, MSBFIRST, SPI_MODE0);
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);

  SPI.begin();

  /* Deep Power-Down 상태면 JEDEC ID가 안 나온다. 먼저 깨운다. */
  select();
  SPI.transfer(CMD_RELEASE_DPD);
  deselect();
  delayMicroseconds(50);

  return probe();
}

bool GD25Q80::probe() {
  select();
  SPI.transfer(CMD_JEDEC_ID);
  uint8_t mfr  = SPI.transfer(0xFF);
  uint8_t type = SPI.transfer(0xFF);
  uint8_t cap  = SPI.transfer(0xFF);
  deselect();

  _jedec = ((uint32_t)mfr << 16) | ((uint32_t)type << 8) | cap;
  _present = (_jedec == JEDEC_AT25SF081 || _jedec == JEDEC_GD25Q80);
  return _present;
}

const char *GD25Q80::chipName() const {
  if (_jedec == JEDEC_AT25SF081) {
    return "AT25SF081";
  }
  if (_jedec == JEDEC_GD25Q80) {
    return "GD25Q80";
  }
  return "unknown";
}

void GD25Q80::dumpInfo(Stream &out) {
  out.print("SPI flash CS=");
  out.print(_cs);
  out.print(" JEDEC=0x");
  if (_jedec < 0x100000UL) {
    out.print('0');
  }
  out.print(_jedec, HEX);
  out.print("  ");
  if (_present) {
    out.print("OK ");
    out.println(chipName());
  } else if (_jedec == 0UL || _jedec == 0x00FFFFFFUL) {
    out.println("NO RESPONSE (MISO 0x00/0xFF)");
  } else {
    out.println("UNKNOWN ID  expect AT25SF081=0x1F8501");
  }
}

uint8_t GD25Q80::readStatus() {
  select();
  SPI.transfer(CMD_READ_STATUS);
  uint8_t st = SPI.transfer(0xFF);
  deselect();
  return st;
}

bool GD25Q80::waitBusy(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (readStatus() & 0x01) {
    SerialFlashUpload_kick();
    if ((millis() - start) >= timeoutMs) {
      return false;
    }
  }
  return true;
}

bool GD25Q80::read(uint32_t addr, uint8_t *buf, uint32_t len) {
  if (buf == NULL || (addr + len) > CAPACITY) {
    return false;
  }
  select();
  SPI.transfer(CMD_READ);
  transferAddr(addr);
  for (uint32_t i = 0; i < len; i++) {
    buf[i] = SPI.transfer(0xFF);
  }
  deselect();
  return true;
}

bool GD25Q80::writeEnable() {
  select();
  SPI.transfer(CMD_WRITE_ENABLE);
  deselect();
  return (readStatus() & 0x02) != 0;
}

bool GD25Q80::write(uint32_t addr, const uint8_t *buf, uint32_t len) {
  if (buf == NULL || (addr + len) > CAPACITY) {
    return false;
  }
  while (len > 0) {
    uint16_t pageOff = (uint16_t)(addr & (PAGE_SIZE - 1));
    uint16_t chunk = PAGE_SIZE - pageOff;
    if (chunk > len) {
      chunk = (uint16_t)len;
    }
    if (!pageProgram(addr, buf, chunk)) {
      return false;
    }
    addr += chunk;
    buf += chunk;
    len -= chunk;
  }
  return true;
}

bool GD25Q80::eraseSector(uint32_t addr) {
  if (addr >= CAPACITY) {
    return false;
  }
  if (!writeEnable()) {
    return false;
  }
  select();
  SPI.transfer(CMD_SECTOR_ERASE);
  transferAddr(addr);
  deselect();
  return waitBusy(500);
}

bool GD25Q80::eraseChip(Stream *progress) {
  if (!writeEnable()) {
    return false;
  }
  select();
  SPI.transfer(CMD_CHIP_ERASE);
  deselect();
  uint32_t start = millis();
  uint32_t lastPing = 0;
  while (readStatus() & 0x01) {
    SerialFlashUpload_kick();
    if (progress != NULL && (millis() - lastPing) >= 400UL) {
      lastPing = millis();
      progress->println("C1DL BUSY");
      progress->flush();
    }
    if ((millis() - start) >= 15000UL) {
      return false;
    }
  }
  return true;
}

bool GD25Q80::pageProgram(uint32_t addr, const uint8_t *buf, uint16_t len) {
  if (buf == NULL || len == 0 || len > PAGE_SIZE) {
    return false;
  }
  if ((addr & (PAGE_SIZE - 1)) + len > PAGE_SIZE) {
    return false;
  }
  if ((addr + len) > CAPACITY) {
    return false;
  }
  if (!writeEnable()) {
    return false;
  }
  select();
  SPI.transfer(CMD_PAGE_PROGRAM);
  transferAddr(addr);
  for (uint16_t i = 0; i < len; i++) {
    SPI.transfer(buf[i]);
  }
  deselect();
  return waitBusy(50);
}

void GD25Q80::select() {
  SPI.beginTransaction(_settings);
  digitalWrite(_cs, LOW);
}

void GD25Q80::deselect() {
  digitalWrite(_cs, HIGH);
  SPI.endTransaction();
}

void GD25Q80::transferAddr(uint32_t addr) {
  SPI.transfer((uint8_t)(addr >> 16));
  SPI.transfer((uint8_t)(addr >> 8));
  SPI.transfer((uint8_t)addr);
}
