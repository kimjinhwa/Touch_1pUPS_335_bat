# -*- coding: utf-8 -*-
"""ESP-Prog 반복 시험.

  python repeat_espprog.py handshake 30
  python repeat_espprog.py image 5
  python repeat_espprog.py firmware 3
  python repeat_espprog.py all
"""
from __future__ import annotations

import sys
import time
from pathlib import Path

import serial

from firmware import find_bossac, kit_dir, run_bossac
from hw import EspProgHw
from pack import build_pack
from protocol import BAUD, list_files_retry, quit_session, send_pack, wait_ready_and_hello

PORT = "COM21"
HAN = kit_dir() / "han"
BIN = kit_dir() / "withScreenSaver_Touch_1pUPS_335_bat_flash.ino.bin"
HELLO_S = 8.0
HELLO_RETRY_S = 20.0


def log(msg: str) -> None:
    print(msg, flush=True)


def open_idle(hw: EspProgHw) -> serial.Serial:
    ser = serial.Serial()
    ser.port = PORT
    ser.baudrate = BAUD
    ser.timeout = 0.3
    ser.write_timeout = 20
    ser.xonxoff = False
    ser.rtscts = False
    ser.dsrdtr = False
    ser.dtr = True
    ser.rts = True
    ser.open()
    hw.apply_idle(ser)
    return ser


def hello(hw: EspProgHw, ser: serial.Serial) -> bool:
    hw.unfreeze()
    hw.apply_idle(ser)
    hw.pulse_reset(ser, log)
    if wait_ready_and_hello(ser, log, HELLO_S):
        hw.freeze_run(ser, log)
        return True
    log("HELLO 재시도")
    hw.pulse_reset(ser, log)
    ok = wait_ready_and_hello(ser, log, HELLO_RETRY_S)
    if ok:
        hw.freeze_run(ser, log)
    return ok


def trial_handshake(hw: EspProgHw) -> str:
    ser = None
    try:
        ser = open_idle(hw)
        if not hello(hw, ser):
            return "HELLO"
        files = list_files_retry(ser, log)
        quit_session(ser, log)
        hw.unfreeze()
        hw.apply_idle(ser)
        return f"ok list={len(files)}"
    except Exception as exc:
        return f"exc {exc}"
    finally:
        if ser is not None:
            try:
                hw.unfreeze()
                hw.apply_idle(ser)
                ser.close()
            except Exception:
                pass


def trial_image(hw: EspProgHw, pack: bytes, expect: int) -> str:
    ser = None
    try:
        ser = open_idle(hw)
        if not hello(hw, ser):
            return "HELLO"
        if not send_pack(ser, pack, log, None):
            return "SEND"
        files = list_files_retry(ser, log)
        quit_session(ser, log)
        hw.unfreeze()
        hw.apply_idle(ser)
        if len(files) != expect:
            return f"LIST {len(files)}!={expect}"
        return f"ok list={len(files)}"
    except Exception as exc:
        return f"exc {exc}"
    finally:
        if ser is not None:
            try:
                hw.unfreeze()
                hw.apply_idle(ser)
                ser.close()
            except Exception:
                pass


def trial_firmware(hw: EspProgHw) -> str:
    ser = None
    try:
        ser = open_idle(hw)
        hw.unfreeze()
        hw.erase_then_reset(ser, log)
        hw.apply_idle(ser)
        ser.close()
        ser = None
        if not BIN.is_file():
            return f"no bin {BIN}"
        if not find_bossac().is_file():
            return "no bossac"
        ok = run_bossac(PORT, BIN, log, None)
        return "ok" if ok else "bossac"
    except Exception as exc:
        return f"exc {exc}"
    finally:
        if ser is not None:
            try:
                hw.apply_idle(ser)
                ser.close()
            except Exception:
                pass


def run_loop(name: str, n: int, fn) -> int:
    ok_n = 0
    fail = []
    t0 = time.perf_counter()
    for i in range(1, n + 1):
        log(f"\n===== {name} {i}/{n} =====")
        t1 = time.perf_counter()
        result = fn()
        dt = time.perf_counter() - t1
        good = result.startswith("ok")
        if good:
            ok_n += 1
        else:
            fail.append((i, result))
        log(f"----- {name} {i}/{n} {result}  {dt:.1f}s")
    log(
        f"\n{name} 결과  {ok_n}/{n}  fail={fail}  "
        f"{time.perf_counter() - t0:.1f}s"
    )
    return 0 if ok_n == n else 1


def main() -> int:
    mode = (sys.argv[1] if len(sys.argv) > 1 else "handshake").lower()
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    hw = EspProgHw()
    log(f"port={PORT} bin={BIN.name} han={HAN} bossac={find_bossac().is_file()}")

    if mode in ("handshake", "hs"):
        return run_loop("handshake", n or 20, lambda: trial_handshake(hw))
    if mode in ("image", "img"):
        pack, listing = build_pack(HAN)
        log(f"pack {len(pack)}B files={len(listing)}")
        return run_loop("image", n or 5, lambda: trial_image(hw, pack, len(listing)))
    if mode in ("firmware", "fw"):
        return run_loop("firmware", n or 2, lambda: trial_firmware(hw))
    if mode == "all":
        rc = 0
        rc |= run_loop("firmware", 2, lambda: trial_firmware(hw))
        rc |= run_loop("handshake", 20, lambda: trial_handshake(hw))
        pack, listing = build_pack(HAN)
        log(f"pack {len(pack)}B files={len(listing)}")
        rc |= run_loop("image", 5, lambda: trial_image(hw, pack, len(listing)))
        return rc
    log("usage: repeat_espprog.py handshake|image|firmware|all [n]")
    return 2


if __name__ == "__main__":
    sys.exit(main())
