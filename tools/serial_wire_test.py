# -*- coding: utf-8 -*-
"""COM5 8N1 바이너리 통과 시험. 펌웨어 SERIAL_WIRE_TEST 와 짝.

  python tools/serial_wire_test.py COM5
"""
from __future__ import annotations

import sys
import time
import zlib

import serial

BAUD = 115200


def log(msg: str) -> None:
    enc = getattr(sys.stdout, "encoding", None) or "utf-8"
    print(msg.encode(enc, "replace").decode(enc, "replace"), flush=True)


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def read_line(ser, timeout: float) -> str:
    ser.timeout = timeout
    raw = ser.readline()
    if not raw:
        return ""
    return raw.decode("ascii", "replace").replace("\ufffd", "?").strip()


def wait_ready(ser, timeout: float = 8.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = read_line(ser, 0.4)
        if not line:
            continue
        log(f"rx {line}")
        if "WIRETEST READY" in line:
            return True
    return False


def send_bin(ser, payload: bytes) -> dict[str, str]:
    ser.reset_input_buffer()
    header = f"BIN {len(payload)}\n".encode("ascii")
    ser.write(header)
    ser.flush()
    time.sleep(0.02)
    ser.write(payload)
    ser.flush()
    fields: dict[str, str] = {}
    deadline = time.time() + 10.0
    while time.time() < deadline:
        line = read_line(ser, 0.5)
        if not line:
            continue
        log(f"rx {line}")
        if line.startswith("GOT "):
            parts = line.split()
            fields["got"] = parts[1]
            fields["want"] = parts[3]
        elif line.startswith("ZEROS "):
            fields["zeros"] = line.split()[1]
        elif line.startswith("X20 "):
            fields["x20"] = line.split()[1]
        elif line.startswith("X40 "):
            fields["x40"] = line.split()[1]
        elif line.startswith("X0A "):
            fields["x0a"] = line.split()[1]
        elif line.startswith("X0D "):
            fields["x0d"] = line.split()[1]
        elif line.startswith("XFF "):
            fields["xff"] = line.split()[1]
        elif line.startswith("CRC "):
            fields["crc"] = line.split()[1]
        elif line.startswith("HEAD"):
            fields["head"] = line[4:].strip()
        elif line == "END":
            break
    return fields


def expect(name: str, payload: bytes, fields: dict[str, str]) -> bool:
    ok = True
    want = str(len(payload))
    if fields.get("got") != want:
        log(f"FAIL {name}: got {fields.get('got')} want {want}")
        ok = False
    got_crc = fields.get("crc")
    exp_crc = str(crc32(payload))
    if got_crc != exp_crc:
        log(f"FAIL {name}: crc fw={got_crc} pc={exp_crc}")
        ok = False
    zeros = payload.count(0)
    x20 = payload.count(0x20)
    x40 = payload.count(0x40)
    if fields.get("zeros") != str(zeros):
        log(f"FAIL {name}: zeros fw={fields.get('zeros')} pc={zeros}")
        ok = False
    if fields.get("x20") != str(x20):
        log(f"FAIL {name}: 0x20 fw={fields.get('x20')} pc={x20}")
        ok = False
    if fields.get("x40") != str(x40):
        log(f"FAIL {name}: 0x40 fw={fields.get('x40')} pc={x40}")
        ok = False
    if ok:
        log(f"PASS {name}: {len(payload)} bytes crc match")
    return ok


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    log(f"open {port} {BAUD} 8N1 binary")
    ser = serial.Serial(
        port=port,
        baudrate=BAUD,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        timeout=0.4,
        write_timeout=5,
        dsrdtr=False,
        rtscts=False,
        xonxoff=False,
    )
    ser.dtr = False
    ser.rts = False
    try:
        if not wait_ready(ser):
            ser.write(b"\n")
            ser.flush()
            if not wait_ready(ser, 4.0):
                log("FAIL no WIRETEST READY — 보드에 wire 펌웨어가 없거나 포트가 다름")
                return 1
        time.sleep(0.4)
        ser.reset_input_buffer()
        ser.write(b"PING\n")
        ser.flush()
        pong = read_line(ser, 2.0)
        log(f"rx {pong}")
        if pong != "PONG":
            log("FAIL PING")
            return 1
        log("PASS PING")

        cases = [
            ("all00", bytes([0x00]) * 64),
            ("all20", bytes([0x20]) * 64),
            ("all40", bytes([0x40]) * 64),
            ("allFF", bytes([0xFF]) * 64),
            ("seq00FF", bytes(range(256))),
            ("mix", bytes([0x00, 0x20, 0x40, 0x0A, 0x0D, 0xFF]) * 128),
        ]
        failed = 0
        for name, payload in cases:
            log(f"--- {name} {len(payload)} bytes ---")
            fields = send_bin(ser, payload)
            if not expect(name, payload, fields):
                failed += 1
            time.sleep(0.15)
        if failed:
            log(f"RESULT FAIL {failed}/{len(cases)}")
            return 2
        log("RESULT PASS all binary cases")
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
