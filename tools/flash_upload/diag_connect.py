# -*- coding: utf-8 -*-
"""HELLO + LIST 진단. 플래시는 지우지 않는다.

  python diag_connect.py COM5
"""
from __future__ import annotations

import platform
import sys
import traceback

import serial
from serial.tools import list_ports

from protocol import (
    BAUD,
    HELLO_WAIT_S,
    close_session_log,
    init_session_log,
    list_files_retry,
    quit_session,
    session_log_path,
    trace,
    wait_ready_and_hello,
)


def log(msg: str) -> None:
    print(msg, flush=True)
    trace("APP", msg)


def main() -> int:
    port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
    log_path = init_session_log()
    print(f"LOG {log_path}", flush=True)
    trace("META", f"host={platform.node()} {platform.platform()}")
    trace("META", f"python={sys.version.split()[0]} pyserial={serial.__version__}")
    ports = [p.device for p in list_ports.comports()]
    log(f"ports={ports}")

    print(f"WAITING_FOR_RESET port={port} baud={BAUD}", flush=True)
    ser = None
    try:
        ser = serial.Serial(
            port=port,
            baudrate=BAUD,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.3,
            write_timeout=5,
            dsrdtr=False,
            rtscts=False,
            xonxoff=False,
        )
        ser.dtr = False
        ser.rts = False
        trace("META", f"port={port} dtr=0 rts=0 xonxoff=0")
    except Exception as exc:
        print(f"COM5_BUSY {exc}", flush=True)
        trace("ERR", str(exc))
        close_session_log()
        return 2

    try:
        if not wait_ready_and_hello(ser, log, HELLO_WAIT_S):
            print("HELLO_FAIL", flush=True)
            return 1
        print("HELLO_OK", flush=True)
        files = list_files_retry(ser, log)
        print(f"LIST_OK n={len(files)}", flush=True)
        for path, size in files:
            log(f"FILE {path} {size}")
        quit_session(ser, log)
        print("QUIT_OK", flush=True)
        print(f"LOG {session_log_path()}", flush=True)
        return 0
    except Exception:
        traceback.print_exc()
        print("DIAG_FAIL", flush=True)
        return 1
    finally:
        if ser is not None:
            try:
                ser.close()
            except Exception:
                pass
        close_session_log()


if __name__ == "__main__":
    raise SystemExit(main())
