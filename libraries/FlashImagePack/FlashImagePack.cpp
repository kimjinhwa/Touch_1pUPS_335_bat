#include "FlashImagePack.h"
#include <string.h>

FlashImagePack::FlashImagePack()
  : _flash(NULL),
    _ok(false),
    _fileCount(0),
    _dataOffset(0) {
}

bool FlashImagePack::headerOk(GD25Q80 &flash) {
  uint8_t hdr[HEADER_SIZE];
  if (!flash.read(0, hdr, HEADER_SIZE)) {
    return false;
  }
  uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
                   ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
  uint16_t ver = (uint16_t)hdr[4] | ((uint16_t)hdr[5] << 8);
  uint16_t count = (uint16_t)hdr[6] | ((uint16_t)hdr[7] << 8);
  uint32_t dataOff = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8) |
                     ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
  if (magic != MAGIC || ver != VERSION || count == 0) {
    return false;
  }
  if (dataOff < (uint32_t)HEADER_SIZE + (uint32_t)count * ENTRY_SIZE) {
    return false;
  }
  if (dataOff >= GD25Q80::CAPACITY) {
    return false;
  }
  return true;
}

bool FlashImagePack::begin(GD25Q80 &flash) {
  _flash = &flash;
  _ok = false;
  _fileCount = 0;
  _dataOffset = 0;

  uint8_t hdr[HEADER_SIZE];
  if (!flash.read(0, hdr, HEADER_SIZE)) {
    return false;
  }
  uint32_t magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) |
                   ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
  uint16_t ver = (uint16_t)hdr[4] | ((uint16_t)hdr[5] << 8);
  _fileCount = (uint16_t)hdr[6] | ((uint16_t)hdr[7] << 8);
  _dataOffset = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8) |
                ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
  if (magic != MAGIC || ver != VERSION || _fileCount == 0) {
    return false;
  }
  if (_dataOffset < (uint32_t)HEADER_SIZE + (uint32_t)_fileCount * ENTRY_SIZE) {
    return false;
  }
  _ok = true;
  return true;
}

bool FlashImagePack::find(const char *path, uint32_t *offset, uint32_t *size) {
  if (!_ok || _flash == NULL || path == NULL) {
    return false;
  }
  char want[PATH_SIZE];
  pathNorm(path, want, PATH_SIZE);

  uint8_t entry[ENTRY_SIZE];
  for (uint16_t i = 0; i < _fileCount; i++) {
    uint32_t addr = (uint32_t)HEADER_SIZE + (uint32_t)i * ENTRY_SIZE;
    if (!_flash->read(addr, entry, ENTRY_SIZE)) {
      return false;
    }
    char rawPath[PATH_SIZE + 1];
    memcpy(rawPath, entry, PATH_SIZE);
    rawPath[PATH_SIZE] = 0;
    char have[PATH_SIZE];
    pathNorm(rawPath, have, PATH_SIZE);
    if (pathEqual(want, have)) {
      uint32_t off = (uint32_t)entry[40] | ((uint32_t)entry[41] << 8) |
                     ((uint32_t)entry[42] << 16) | ((uint32_t)entry[43] << 24);
      uint32_t sz = (uint32_t)entry[44] | ((uint32_t)entry[45] << 8) |
                    ((uint32_t)entry[46] << 16) | ((uint32_t)entry[47] << 24);
      if (offset) {
        *offset = off;
      }
      if (size) {
        *size = sz;
      }
      return true;
    }
  }
  return false;
}

bool FlashImagePack::getEntry(uint16_t index, char *pathOut, uint8_t pathOutLen, uint32_t *offset, uint32_t *size) {
  if (!_ok || _flash == NULL || index >= _fileCount || pathOut == NULL || pathOutLen < 2) {
    return false;
  }
  uint8_t entry[ENTRY_SIZE];
  uint32_t addr = (uint32_t)HEADER_SIZE + (uint32_t)index * ENTRY_SIZE;
  if (!_flash->read(addr, entry, ENTRY_SIZE)) {
    return false;
  }
  uint8_t n = 0;
  while (n < PATH_SIZE && entry[n] != 0 && n < (uint8_t)(pathOutLen - 1)) {
    pathOut[n] = (char)entry[n];
    n++;
  }
  pathOut[n] = 0;
  uint32_t off = (uint32_t)entry[40] | ((uint32_t)entry[41] << 8) |
                 ((uint32_t)entry[42] << 16) | ((uint32_t)entry[43] << 24);
  uint32_t sz = (uint32_t)entry[44] | ((uint32_t)entry[45] << 8) |
                ((uint32_t)entry[46] << 16) | ((uint32_t)entry[47] << 24);
  if (offset) {
    *offset = off;
  }
  if (size) {
    *size = sz;
  }
  return true;
}

bool FlashImagePack::read(uint32_t addr, uint8_t *buf, uint32_t len) {
  if (_flash == NULL) {
    return false;
  }
  return _flash->read(addr, buf, len);
}

void FlashImagePack::pathNorm(const char *src, char *dst, uint8_t dstLen) {
  uint8_t n = 0;
  if (dstLen == 0) {
    return;
  }
  while (src[n] != 0 && n < (uint8_t)(dstLen - 1)) {
    char c = src[n];
    if (c == '\\') {
      c = '/';
    } else if (c >= 'A' && c <= 'Z') {
      c = (char)(c - 'A' + 'a');
    }
    dst[n] = c;
    n++;
  }
  dst[n] = 0;
}

bool FlashImagePack::pathEqual(const char *a, const char *b) {
  while (*a && *b) {
    if (*a != *b) {
      return false;
    }
    a++;
    b++;
  }
  return *a == *b;
}
