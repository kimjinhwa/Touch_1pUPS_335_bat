# -*- coding: utf-8 -*-
"""Due 업로드용 모드버스 릴레이 (COM7 9600).

  기능코드 06, 슬레이브 기본 1
  레지스터 1 = RESET  (0x0100 ON, 0x0200 OFF)
  레지스터 2 = ERASE  (0x0100 ON, 0x0200 OFF)
"""
from __future__ import annotations

import sys
import time

import serial

PORT = "COM7"
BAUD = 9600
SLAVE = 1
ADDR_RESET = 1
ADDR_ERASE = 2
VAL_ON = 0x0100
VAL_OFF = 0x0200


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc


def write_reg(ser: serial.Serial, slave: int, addr: int, value: int) -> bytes:
    pdu = bytes(
        [
            slave,
            0x06,
            (addr >> 8) & 0xFF,
            addr & 0xFF,
            (value >> 8) & 0xFF,
            value & 0xFF,
        ]
    )
    c = crc16(pdu)
    frame = pdu + bytes([c & 0xFF, (c >> 8) & 0xFF])
    ser.reset_input_buffer()
    ser.write(frame)
    ser.flush()
    time.sleep(0.2)
    return ser.read(16)


def pulse(ser: serial.Serial, name: str, addr: int, hold: float = 1.5) -> None:
    print(f"{name} ON  (06 {addr:02d} 0x{VAL_ON:04X})", flush=True)
    resp = write_reg(ser, SLAVE, addr, VAL_ON)
    print(f"  tx ok  rx={resp.hex() if resp else '(없음)'}", flush=True)
    time.sleep(hold)
    print(f"{name} OFF (06 {addr:02d} 0x{VAL_OFF:04X})", flush=True)
    resp = write_reg(ser, SLAVE, addr, VAL_OFF)
    print(f"  tx ok  rx={resp.hex() if resp else '(없음)'}", flush=True)
    time.sleep(0.8)


def main() -> int:
    action = (sys.argv[1] if len(sys.argv) > 1 else "both").lower()
    print(f"open {PORT} {BAUD} 8N1  slave={SLAVE}  action={action}", flush=True)
    ser = serial.Serial(
        port=PORT,
        baudrate=BAUD,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.4,
        write_timeout=2,
        dsrdtr=False,
        rtscts=False,
    )
    try:
        if action in ("reset", "both"):
            print("--- RESET 클릭 ---", flush=True)
            pulse(ser, "RESET", ADDR_RESET, hold=0.4)
        if action in ("erase", "both"):
            print("--- ERASE 클릭 ---", flush=True)
            pulse(ser, "ERASE", ADDR_ERASE, hold=0.4)
        print("끝", flush=True)
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
