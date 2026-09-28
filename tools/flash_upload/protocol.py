# -*- coding: utf-8 -*-
"""시리얼 세션 프로토콜 — 펌웨어 SerialFlashUpload 과 동일.

명령은 ASCII, 데이터는 BEGIN/PUT 뒤 길이만큼 바이너리.
  C1DL BEGIN <size> <crc32> → GO 후 size 바이트 → DONE
"""

from __future__ import annotations

import time
from collections.abc import Callable

from pack import crc32

CHUNK = 16
CHUNK_MAX = 16
BAUD = 115200


def _read_line(ser, timeout: float) -> str:
    ser.timeout = timeout
    raw = ser.readline()
    if not raw:
        return ""
    return raw.decode("ascii", "replace").replace("\ufffd", "?").strip()


def wait_ready_and_hello(ser, log: Callable[[str], None], timeout: float = 20.0) -> bool:
    log("단말 Serial(COM5) 115200 C1DL HELLO 대기")
    log("시리얼 모니터는 닫고 이 프로그램만. 연 뒤에 단말을 리셋하세요")
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = _read_line(ser, 0.3)
        if not line:
            continue
        log(line)
        if line.startswith("C1DL HELLO") or line.startswith("C1DL READY"):
            ser.write(b"C1DL HELLO\n")
            ser.flush()
            mode_deadline = time.time() + 3.0
            while time.time() < mode_deadline:
                resp = _read_line(ser, 0.3)
                if not resp:
                    continue
                log(resp)
                if resp.startswith("C1DL MODE"):
                    time.sleep(0.2)
                    return True
                if resp.startswith("C1DL HELLO") or resp.startswith("C1DL READY"):
                    ser.write(b"C1DL HELLO\n")
                    ser.flush()
            log("HELLO 응답 없음")
            return False
        if line.startswith("C1DL BOOT"):
            log("단말이 이미 정상 부팅으로 넘어갔습니다. 다시 리셋하세요.")
    return False


def list_files(ser, log: Callable[[str], None], timeout: float = 20.0) -> list[tuple[str, int]]:
    ser.reset_input_buffer()
    ser.write(b"C1DL LIST\n")
    ser.flush()
    files: list[tuple[str, int]] = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = _read_line(ser, 0.5)
        if not line:
            continue
        if line.startswith("C1DL RX"):
            log(line)
            continue
        if line.startswith("C1DL LISTACK"):
            continue
        if line.startswith("C1DL UNK"):
            log(line)
            continue
        if line.startswith("C1DL FILE "):
            body = line[10:]
            path, _, size_s = body.rpartition(" ")
            try:
                files.append((path, int(size_s)))
            except ValueError:
                log(f"목록 파싱 실패: {line}")
            continue
        if line.startswith("C1DL LIST END"):
            return files
        if line.startswith("C1DL LIST "):
            log(line)
            continue
        log(line)
    log("LIST 타임아웃")
    return files


def _is_keepalive(line: str) -> bool:
    return line.startswith(
        (
            "C1DL BUSY",
            "C1DL ERASE",
            "C1DL BEGINACK",
            "C1DL PUTACK",
            "C1DL LISTACK",
            "C1DL RX",
        )
    )


def _wait_ok_line(ser, log: Callable[[str], None], ok_prefix: str, timeout: float) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = _read_line(ser, 0.5)
        if not line:
            continue
        log(line)
        if line.startswith("C1DL FAIL"):
            return False
        if line.startswith("C1DL UNK"):
            log("명령 거부 (세션 잔여 바이트). 다시 리셋하세요.")
            return False
        if _is_keepalive(line):
            deadline = time.time() + timeout
            continue
        if line.startswith(ok_prefix):
            return True
    log(f"{ok_prefix} 타임아웃")
    return False


def _wait_chunk_ack(
    ser, log: Callable[[str], None], offset: int, nbytes: int, quiet: bool = True
) -> tuple[str, int | None]:
    expect = offset + nbytes
    deadline = time.time() + 8.0
    while time.time() < deadline:
        line = _read_line(ser, 0.4)
        if not line:
            continue
        parts = line.split()
        if len(parts) >= 3 and parts[0] == "C1DL" and parts[1] == "A":
            try:
                got = int(parts[2])
            except ValueError:
                log(line)
                continue
            if got == expect:
                return ("A", got)
            if not quiet:
                log(line)
            if got < expect:
                return ("SEQ", got)
            continue
        if len(parts) >= 5 and parts[0] == "C1DL" and parts[1] == "N" and parts[2] == "SEQ":
            if not quiet:
                log(line)
            try:
                want = int(parts[4])
            except ValueError:
                return ("N", None)
            return ("SEQ", want)
        if line.startswith("C1DL N"):
            if not quiet:
                log(line)
            return ("N", None)
        if line.startswith("C1DL FAIL"):
            log(line)
            return ("FAIL", None)
        if not quiet:
            log(f"chunk {offset}: {line}")
    return ("", None)


def _note(log: Callable[[str], None], quiet: bool, naks: list[int], msg: str) -> None:
    naks[0] += 1
    if (not quiet) or naks[0] <= 4 or (naks[0] % 40) == 0:
        log(msg)


def _send_chunks(
    ser,
    data: bytes,
    log: Callable[[str], None],
    progress: Callable[[int, int], None] | None = None,
    already: int = 0,
    total: int | None = None,
    chunk_size: int = CHUNK,
    quiet: bool = True,
    page_pause: float = 0.0,
) -> bool:
    if total is None:
        total = len(data)
    if chunk_size < 1:
        chunk_size = 1
    if chunk_size > CHUNK_MAX:
        chunk_size = CHUNK_MAX
    sent = 0
    last_log = -1
    naks = [0]
    t0 = time.perf_counter()
    while sent < len(data):
        chunk = data[sent : sent + chunk_size]
        line = f"C1DL D {sent} {chunk.hex()} {crc32(chunk)}\r\n".encode("ascii")
        retries = 0
        while True:
            ser.write(line)
            ser.flush()
            ack, want = _wait_chunk_ack(ser, log, sent, len(chunk), quiet)
            if ack == "A":
                break
            if ack == "FAIL":
                return False
            if ack == "SEQ" and want is not None and want <= sent:
                if want < sent:
                    _note(log, quiet, naks, f"단말 위치 맞춤 {sent} -> {want}")
                    sent = want
                    chunk = data[sent : sent + chunk_size]
                    line = f"C1DL D {sent} {chunk.hex()} {crc32(chunk)}\r\n".encode("ascii")
                try:
                    ser.reset_input_buffer()
                except Exception:
                    pass
                retries = 0
                continue
            retries += 1
            if ack == "N" or ack == "SEQ":
                _note(log, quiet, naks, f"NAK offset={sent} retry={retries}")
            else:
                _note(log, quiet, naks, f"ACK 없음 offset={sent} retry={retries}")
            if retries >= 16:
                log("재시도 초과")
                return False
        sent += len(chunk)
        if page_pause > 0 and sent % 256 == 0:
            time.sleep(page_pause)
        if progress:
            progress(already + sent, total)
        if sent == len(data) or sent // 2048 != last_log:
            last_log = sent // 2048
            pct = int(sent * 100 / total) if total else 0
            log(f"진행 {pct}%  {sent}/{total}")
    elapsed = time.perf_counter() - t0
    bps = (len(data) / elapsed) if elapsed > 0 else 0
    log(f"청크 {chunk_size}B  {len(data)}바이트  {elapsed:.1f}s  {bps:.0f} B/s  NAK {naks[0]}")
    return True


def _send_binary(
    ser,
    data: bytes,
    log: Callable[[str], None],
    progress: Callable[[int, int], None] | None = None,
    already: int = 0,
    total: int | None = None,
) -> bool:
    if total is None:
        total = len(data)
    sent = 0
    last_log = -1
    t0 = time.perf_counter()
    while sent < len(data):
        n = min(1024, len(data) - sent)
        ser.write(data[sent : sent + n])
        sent += n
        if progress:
            progress(already + sent, total)
        if sent == len(data) or sent // 16384 != last_log:
            last_log = sent // 16384
            pct = int(sent * 100 / total) if total else 0
            log(f"진행 {pct}%  {sent}/{total}")
    ser.flush()
    elapsed = time.perf_counter() - t0
    bps = (len(data) / elapsed) if elapsed > 0 else 0
    log(f"바이너리 {len(data)}바이트  {elapsed:.1f}s  {bps:.0f} B/s")
    return True


def send_pack(
    ser,
    pack: bytes,
    log: Callable[[str], None],
    progress: Callable[[int, int], None] | None = None,
    chunk_size: int = CHUNK,
    quiet: bool = True,
    page_pause: float = 0.0,
) -> bool:
    total_crc = crc32(pack)
    begin = f"C1DL BEGIN {len(pack)} {total_crc}\n".encode("ascii")
    log(f"BEGIN size={len(pack)} crc={total_crc} chunk={chunk_size}")
    time.sleep(0.4)
    ser.write(begin)
    ser.flush()

    if not _wait_ok_line(ser, log, "C1DL GO", 25.0):
        log("GO 없음 (erase 실패 또는 무응답)")
        return False
    time.sleep(0.12)
    ser.reset_input_buffer()
    if not _send_binary(ser, pack, log, progress):
        return False
    wait_s = max(20.0, len(pack) / 8000.0 + 10.0)
    return _wait_ok_line(ser, log, "C1DL DONE", wait_s)


def send_file(
    ser,
    path: str,
    data: bytes,
    log: Callable[[str], None],
    progress: Callable[[int, int], None] | None = None,
    already: int = 0,
    total: int | None = None,
    chunk_size: int = CHUNK,
    quiet: bool = True,
    page_pause: float = 0.0,
) -> bool:
    total_crc = crc32(data)
    cmd = f"C1DL PUT {path} {len(data)} {total_crc}\n".encode("ascii")
    ser.reset_input_buffer()
    ser.write(cmd)
    ser.flush()
    log(f"PUT {path} size={len(data)} crc={total_crc}")

    if not _wait_ok_line(ser, log, "C1DL PUTGO", 5.0):
        return False
    time.sleep(0.12)
    ser.reset_input_buffer()
    if not _send_binary(
        ser,
        data,
        log,
        progress,
        already,
        total if total is not None else len(data),
    ):
        return False
    wait_s = max(10.0, len(data) / 8000.0 + 8.0)
    return _wait_ok_line(ser, log, "C1DL PUTOK", wait_s)


def quit_session(ser, log: Callable[[str], None]) -> None:
    try:
        ser.write(b"C1DL QUIT\n")
        ser.flush()
        line = _read_line(ser, 1.0)
        if line:
            log(line)
    except Exception as exc:
        log(f"QUIT: {exc}")
