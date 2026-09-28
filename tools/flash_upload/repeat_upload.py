# -*- coding: utf-8 -*-
"""전체 팩 반복 업로드. COM5 열고 리셋 릴레이를 자동으로 누른다.

  python repeat_upload.py 10
"""
from __future__ import annotations

import subprocess
import sys
import threading
import time
from pathlib import Path

import serial

from pack import build_pack
from protocol import BAUD, list_files, quit_session, send_pack, wait_ready_and_hello

SKETCH = Path(__file__).resolve().parents[2]
RELAY = SKETCH / "tools" / "due_relay.py"


def log(msg: str) -> None:
    enc = getattr(sys.stdout, "encoding", None) or "utf-8"
    print(msg.encode(enc, "replace").decode(enc, "replace"), flush=True)


def log_quiet(msg: str) -> None:
    if msg.startswith(("C1DL BUSY", "진행", "progress")):
        return
    log(msg)


def pulse_reset() -> None:
    time.sleep(0.6)
    subprocess.run(
        [sys.executable, str(RELAY), "reset"],
        cwd=str(SKETCH),
        check=False,
    )


def open_port(port: str):
    ser = serial.Serial(
        port=port,
        baudrate=BAUD,
        timeout=0.3,
        write_timeout=5,
        dsrdtr=False,
        rtscts=False,
        xonxoff=False,
    )
    ser.dtr = False
    ser.rts = False
    return ser


def one_trial(port: str, pack: bytes, expect_files: int) -> tuple[str, float, str]:
    t0 = time.perf_counter()
    ser = None
    why = ""
    try:
        ser = open_port(port)
        threading.Thread(target=pulse_reset, daemon=True).start()
        if not wait_ready_and_hello(ser, log_quiet, 20.0):
            return ("handshake", time.perf_counter() - t0, "HELLO 실패")
        ser.write(b"\n")
        ser.flush()
        time.sleep(0.25)
        ser.reset_input_buffer()
        if not send_pack(ser, pack, log_quiet, None):
            return ("send", time.perf_counter() - t0, "전송/CRC 실패")
        time.sleep(0.4)
        files = list_files(ser, log_quiet)
        quit_session(ser, log_quiet)
        if len(files) != expect_files:
            return ("list", time.perf_counter() - t0, f"목록 {len(files)} != {expect_files}")
        return ("ok", time.perf_counter() - t0, "")
    except Exception as exc:
        return ("exc", time.perf_counter() - t0, str(exc))
    finally:
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass
        why = why


def main() -> int:
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    port = sys.argv[2] if len(sys.argv) > 2 else "COM5"
    folder = Path(sys.argv[3]) if len(sys.argv) > 3 else SKETCH / "han"
    pack, listing = build_pack(folder)
    log(f"repeat {n}  pack {len(pack)} bytes, {len(listing)} files  {port}")
    results: list[tuple[int, str, float, str]] = []
    for i in range(1, n + 1):
        log(f"===== {i}/{n} =====")
        status, elapsed, detail = one_trial(port, pack, len(listing))
        results.append((i, status, elapsed, detail))
        extra = f"  {detail}" if detail else ""
        log(f"trial {i} {status} {elapsed:.1f}s{extra}")
        time.sleep(1.2)
    ok = sum(1 for r in results if r[1] == "ok")
    log("===== SUMMARY =====")
    log(f"ok {ok}/{n}")
    for i, status, elapsed, detail in results:
        extra = f" {detail}" if detail else ""
        log(f"  {i:2d}  {status:10s}  {elapsed:6.1f}s{extra}")
    return 0 if ok == n else 2


if __name__ == "__main__":
    sys.exit(main())
