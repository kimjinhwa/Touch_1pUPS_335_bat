# -*- coding: utf-8 -*-
"""한 번 업로드: COM5 Serial 핸드셰이크 후 han 팩 전송.

예:
  python upload_once.py COM5
  python upload_once.py COM5 --files intro/enter.raw main/setup_btn.raw control/esc.raw --chunk 16
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import serial

from pack import build_pack, build_pack_from_items
from protocol import BAUD, CHUNK, list_files, quit_session, send_pack, wait_ready_and_hello

SKETCH = Path(__file__).resolve().parents[2]


def log(msg: str) -> None:
    enc = getattr(sys.stdout, "encoding", None) or "utf-8"
    print(msg.encode(enc, "replace").decode(enc, "replace"), flush=True)


def progress(sent: int, total: int) -> None:
    if sent == total or sent % 16384 < 32:
        log(f"progress {sent}/{total}")


def _open(port: str):
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


def _items_from_files(folder: Path, rels: list[str]) -> list[tuple[str, Path]]:
    items: list[tuple[str, Path]] = []
    for rel in rels:
        src = folder / rel
        if not src.is_file():
            raise FileNotFoundError(str(src))
        stored = f"{folder.name}/{Path(rel).as_posix()}"
        stored = stored.rsplit(".", 1)[0] + ".RAW"
        items.append((stored, src))
    return items


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("port", nargs="?", default="COM5")
    ap.add_argument("folder", nargs="?", default=str(SKETCH / "han"))
    ap.add_argument("--files", nargs="+", help="폴더 기준 상대 경로. 지정하면 그 파일만 팩으로 올린다")
    ap.add_argument("--chunk", type=int, default=CHUNK)
    ap.add_argument("--keep", action="store_true", help="QUIT 하지 않고 포트만 닫는다")
    args = ap.parse_args()

    folder = Path(args.folder)
    if args.files:
        pack, listing = build_pack_from_items(_items_from_files(folder, args.files))
    else:
        pack, listing = build_pack(folder)
    log(f"open {args.port} {BAUD}")
    log(f"pack {len(pack)} bytes, {len(listing)} files, chunk {args.chunk}")
    ser = _open(args.port)
    try:
        log("waiting HELLO - reset the device now")
        if not wait_ready_and_hello(ser, log, 20.0):
            log("FAIL handshake")
            return 1
        log("MODE ok")
        ser.write(b"\n")
        ser.flush()
        time.sleep(0.25)
        ser.reset_input_buffer()
        files = list_files(ser, log)
        log(f"device files before send {len(files)}")
        time.sleep(0.5)
        log("sending pack")
        if not send_pack(ser, pack, log, progress, chunk_size=args.chunk, quiet=True):
            log("FAIL send")
            return 2
        time.sleep(0.4)
        files = list_files(ser, log)
        log(f"device files {len(files)}")
        if not args.keep:
            quit_session(ser, log)
        log("OK")
        return 0
    finally:
        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
