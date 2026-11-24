# -*- coding: utf-8 -*-
"""bossac 펌웨어 기록. ESP-Prog 하드웨어 ERASE/RESET 이후에만 호출."""
from __future__ import annotations

import re
import subprocess
import sys
from collections.abc import Callable
from pathlib import Path

PCT = re.compile(r"(\d+)\s*%")
KEEP = (
    "Device",
    "Chip ID",
    "Version",
    "Erase",
    "Write",
    "Verify",
    "Done",
    "done",
    "Set boot",
    "CPU reset",
    "Connected",
    "No device",
    "SAM-BA",
    "failed",
    "Failed",
    "Error",
    "error",
)


def kit_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys.executable).resolve().parent
    return Path(__file__).resolve().parents[2] / "upload"


def find_bossac() -> Path:
    return kit_dir() / "bossac.exe"


def list_firmware_bins() -> list[Path]:
    folder = kit_dir()
    if not folder.is_dir():
        return []
    bins = sorted(folder.glob("*flash*.ino.bin"))
    if not bins:
        bins = sorted(folder.glob("*.ino.bin"))
    return bins


def run_bossac(
    port: str,
    bin_path: Path,
    log: Callable[[str], None],
    progress: Callable[[int, int], None] | None = None,
) -> bool:
    bossac = find_bossac()
    if not bossac.is_file():
        log(f"bossac 없음: {bossac}")
        return False
    if not bin_path.is_file():
        log(f"bin 없음: {bin_path}")
        return False
    cmd = [
        str(bossac),
        "-i",
        f"--port={port}",
        "-U",
        "false",
        "-e",
        "-w",
        "-b",
        "-v",
        str(bin_path),
        "-R",
    ]
    log("bossac " + " ".join(cmd[1:]))
    flags = 0
    if sys.platform == "win32":
        flags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
        creationflags=flags,
        cwd=str(bossac.parent),
    )
    assert proc.stdout is not None
    for raw in proc.stdout:
        line = raw.strip()
        if not line:
            continue
        m = PCT.search(line)
        if m and progress is not None:
            progress(int(m.group(1)), 100)
        if line.startswith(("readWord", "writeWord", "write(", "read(", "go(")):
            continue
        if any(line.startswith(p) or p in line for p in KEEP) or m:
            if "[" in line and "%" in line:
                log(line.replace("\r", "").strip())
            else:
                log(line)
    rc = proc.wait()
    log(f"bossac 종료 {rc}")
    return rc == 0
