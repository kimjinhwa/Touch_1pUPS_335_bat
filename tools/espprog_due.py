# -*- coding: utf-8 -*-
"""ESP-Prog (FT2232 COM21) -> custom Due RESET/ERASE diagnostics + upload.

ESP-Prog auto-program (NPN on FT_nDTR / FT_nRTS):
  host DTR True  -> FT_nDTR Low  -> Q1 off -> ESP_EN HIGH  -> RESET released
  host DTR False -> FT_nDTR High -> Q1 on  -> ESP_EN LOW   -> RESET asserted
  host RTS True  -> FT_nRTS Low  -> Q2 off -> IO0 HIGH
  host RTS False -> FT_nRTS High -> Q2 on  -> IO0 LOW

User inverter on IO0 -> Due ERASE (active HIGH, needs >= 220 ms):
  RTS True  -> ERASE LOW
  RTS False -> ERASE HIGH

Idle (chip running, no erase): DTR=True, RTS=True
16U2-style sequence: ERASE 250ms, gap, RESET 80ms, then keep idle.
"""
from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import serial
from serial.win32 import DCB, GetCommState
import ctypes

PORT = "COM21"
BAUD = 115200
ERASE_S = 0.28  # SAM3X min 220 ms
RESET_S = 0.08
GAP_S = 0.05

ROOT = Path(__file__).resolve().parents[1]
BOSSAC = ROOT / "upload" / "bossac.exe"
DEFAULT_BIN = ROOT / "upload" / "withScreenSaver_Touch_1pUPS_335_bat_flash.ino.bin"


def open_port(dtr: bool = True, rts: bool = True) -> serial.Serial:
    ser = serial.Serial()
    ser.port = PORT
    ser.baudrate = BAUD
    ser.bytesize = serial.EIGHTBITS
    ser.parity = serial.PARITY_NONE
    ser.stopbits = serial.STOPBITS_ONE
    ser.timeout = 0.2
    ser.write_timeout = 2
    ser.xonxoff = False
    ser.rtscts = False
    ser.dsrdtr = False
    ser.dtr = dtr
    ser.rts = rts
    ser.open()
    ser.dtr = dtr
    ser.rts = rts
    ser.reset_input_buffer()
    return ser


def dcb_dump(ser: serial.Serial) -> str:
    dcb = DCB()
    dcb.DCBlength = ctypes.sizeof(DCB)
    if not GetCommState(ser._port_handle, ctypes.byref(dcb)):
        return "GetCommState failed"
    def hx(v):
        if isinstance(v, (bytes, bytearray)):
            return v.hex()
        return f"{int(v) & 0xFF:02x}"
    return (
        f"baud={dcb.BaudRate} bytesize={dcb.ByteSize} "
        f"fOutX={dcb.fOutX} fInX={dcb.fInX} "
        f"fDtrControl={dcb.fDtrControl} fRtsControl={dcb.fRtsControl} "
        f"fOutxCtsFlow={dcb.fOutxCtsFlow} fOutxDsrFlow={dcb.fOutxDsrFlow} "
        f"XonChar=0x{hx(dcb.XonChar)} XoffChar=0x{hx(dcb.XoffChar)}"
    )


def ping_samba(ser: serial.Serial, label: str) -> bool:
    ser.reset_input_buffer()
    ser.write(b"\x80")
    ser.flush()
    time.sleep(0.05)
    a = ser.read(16)
    ser.write(b"\x80")
    ser.flush()
    time.sleep(0.05)
    b = ser.read(16)
    ser.write(b"#")
    ser.flush()
    time.sleep(0.08)
    c = ser.read(16)
    ok = (b"\x80" in a or b"\x80" in b) or (b">" in c) or (b"\n\r" in c)
    print(
        f"  SAM-BA {label}: 80a={a.hex() or '-'} 80b={b.hex() or '-'} "
        f"hash={c!r}  {'OK' if ok else 'NO'}",
        flush=True,
    )
    return ok


def idle(ser: serial.Serial) -> None:
    ser.dtr = True
    ser.rts = True


def pulse_reset(ser: serial.Serial) -> None:
    print(f"  pulse RESET  DTR=False {RESET_S:.3f}s (ESP_EN low)", flush=True)
    ser.dtr = False
    ser.rts = True
    time.sleep(RESET_S)
    ser.dtr = True
    ser.rts = True
    time.sleep(0.15)


def pulse_erase(ser: serial.Serial) -> None:
    print(f"  pulse ERASE  RTS=False {ERASE_S:.3f}s (IO0 low -> ERASE high)", flush=True)
    ser.dtr = True
    ser.rts = False
    time.sleep(ERASE_S)
    ser.dtr = True
    ser.rts = True
    time.sleep(0.15)


def due_16u2_sequence(ser: serial.Serial) -> None:
    print("  16U2 sequence: ERASE then RESET, idle after", flush=True)
    idle(ser)
    time.sleep(0.05)
    ser.dtr = True
    ser.rts = False
    time.sleep(ERASE_S)
    ser.rts = True
    time.sleep(GAP_S)
    ser.dtr = False
    time.sleep(RESET_S)
    ser.dtr = True
    ser.rts = True
    time.sleep(0.2)


def cmd_diag() -> int:
    print(f"open {PORT} {BAUD}  idle=DTR True / RTS True", flush=True)
    ser = open_port(True, True)
    try:
        print(f"  pyserial dtr={ser.dtr} rts={ser.rts}", flush=True)
        print(f"  DCB {dcb_dump(ser)}", flush=True)
        ping_samba(ser, "idle (no pulse)")

        print("--- RESET only ---", flush=True)
        pulse_reset(ser)
        ping_samba(ser, "after RESET")

        print("--- ERASE only (will wipe SAM3X flash if inverter is wired) ---", flush=True)
        pulse_erase(ser)
        ping_samba(ser, "after ERASE, no reset")

        print("--- RESET after ERASE ---", flush=True)
        pulse_reset(ser)
        ping_samba(ser, "after ERASE+RESET")

        print("--- 16U2 ERASE then RESET ---", flush=True)
        due_16u2_sequence(ser)
        ok = ping_samba(ser, "after 16U2 sequence")
        idle(ser)
        print(f"  leave port idle dtr={ser.dtr} rts={ser.rts}", flush=True)
        return 0 if ok else 2
    finally:
        try:
            idle(ser)
        except Exception:
            pass
        ser.close()


def cmd_upload(bin_path: Path, verify: bool) -> int:
    if not BOSSAC.is_file():
        print(f"bossac missing: {BOSSAC}", file=sys.stderr)
        return 1
    if not bin_path.is_file():
        print(f"bin missing: {bin_path}", file=sys.stderr)
        return 1

    print(f"pre: 16U2 erase/reset on {PORT}, then bossac {bin_path.name}", flush=True)
    ser = open_port(True, True)
    try:
        print(f"  DCB {dcb_dump(ser)}", flush=True)
        due_16u2_sequence(ser)
        ping_samba(ser, "pre-bossac")
        idle(ser)
        print(f"  close idle dtr={ser.dtr} rts={ser.rts}", flush=True)
    finally:
        idle(ser)
        ser.close()

    args = [str(BOSSAC), "-i", "-d", f"--port={PORT}", "-U", "false", "-e", "-w", "-b"]
    if verify:
        args.append("-v")
    args += [str(bin_path), "-R"]
    print("run:", " ".join(args), flush=True)
    rc = subprocess.call(args)
    print(f"bossac exit={rc}", flush=True)
    return rc


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("action", choices=("diag", "upload", "upload-noverify"))
    p.add_argument("--bin", default=str(DEFAULT_BIN))
    args = p.parse_args()
    if args.action == "diag":
        return cmd_diag()
    return cmd_upload(Path(args.bin), verify=(args.action == "upload"))


if __name__ == "__main__":
    sys.exit(main())
