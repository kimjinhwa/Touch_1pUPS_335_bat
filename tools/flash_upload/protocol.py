# -*- coding: utf-8 -*-
"""시리얼 세션 프로토콜 — 펌웨어 SerialFlashUpload 과 동일.

명령은 ASCII, 데이터는 BEGIN/PUT 뒤 길이만큼 바이너리.
  C1DL BEGIN <size> <crc32> → GO 후 256B마다 C1DL P <offset> → DONE
"""

from __future__ import annotations

import sys
import time
from collections.abc import Callable
from datetime import datetime
from pathlib import Path

from pack import crc32

CHUNK = 16
CHUNK_MAX = 16
BAUD = 115200
HELLO_WAIT_S = 120.0
LIST_TIMEOUT_S = 2.0
LIST_TRIES = 5

_log_fp = None
_log_path: Path | None = None
_t0 = 0.0
_rx_buf = bytearray()


def reset_rx_buf() -> None:
    """시리얼 타임아웃을 건드리지 않는다. FTDI SetCommState 가 DTR을 떨어뜨린다."""
    global _rx_buf
    _rx_buf = bytearray()


def init_session_log(path: str | Path | None = None) -> Path:
    global _log_fp, _log_path, _t0
    close_session_log()
    if path is None:
        log_dir = Path(__file__).resolve().parent / "logs"
        log_dir.mkdir(parents=True, exist_ok=True)
        path = log_dir / datetime.now().strftime("session_%Y%m%d_%H%M%S.txt")
    _log_path = Path(path)
    _t0 = time.perf_counter()
    _log_fp = open(_log_path, "w", encoding="utf-8", newline="\n")
    trace("META", f"python={sys.version.split()[0]}")
    return _log_path


def close_session_log() -> None:
    global _log_fp
    if _log_fp is None:
        return
    try:
        _log_fp.close()
    except Exception:
        pass
    _log_fp = None


def session_log_path() -> Path | None:
    return _log_path


def trace(kind: str, msg: str) -> None:
    if _log_fp is None:
        return
    dt = time.perf_counter() - _t0
    try:
        _log_fp.write(f"{dt:9.3f} {kind:<4} {msg}\n")
        _log_fp.flush()
    except Exception:
        pass


def _read_line(ser, timeout: float) -> str:
    """ser.timeout 을 바꾸지 않는다. Windows FTDI 는 재설정 때 DTR 펄스가 난다."""
    global _rx_buf
    deadline = time.time() + timeout
    while True:
        nl = _rx_buf.find(b"\n")
        if nl >= 0:
            raw = bytes(_rx_buf[: nl + 1])
            del _rx_buf[: nl + 1]
            text = raw.decode("ascii", "replace").replace("\ufffd", "?").strip()
            if text.startswith("C1DL") or all(
                32 <= b < 127 or b in (9,) for b in raw.rstrip(b"\r\n")
            ):
                trace("RX", text)
            else:
                trace("RX", f"NONASCII {len(raw)}B {raw[:24].hex()}")
            return text
        remain = deadline - time.time()
        if remain <= 0:
            return ""
        n = int(getattr(ser, "in_waiting", 0) or 0)
        if n <= 0:
            time.sleep(min(0.02, remain))
            continue
        chunk = ser.read(n)
        if chunk:
            _rx_buf.extend(chunk)


def _write_cmd(ser, data: bytes) -> None:
    trace("TX", data.decode("ascii", "replace").strip())
    ser.write(data)
    ser.flush()


def _discard_rx(ser, log: Callable[[str], None], why: str) -> bytes:
    dumped = b""
    reset_rx_buf()
    try:
        n = int(getattr(ser, "in_waiting", 0) or 0)
        if n > 0:
            dumped = ser.read(n)
    except Exception as exc:
        log(f"RX discard 실패 ({why}): {exc}")
        return b""
    if dumped:
        preview = dumped[:32].hex()
        ascii_p = dumped[:80].decode("ascii", "replace")
        msg = f"RX DISCARD {why} {len(dumped)}B hex={preview} ascii={ascii_p!r}"
        log(msg)
        trace("RX", msg)
    else:
        trace("RX", f"DISCARD {why} empty")
    return dumped


def wait_ready_and_hello(
    ser,
    log: Callable[[str], None],
    timeout: float = HELLO_WAIT_S,
    on_tick: Callable[[int], None] | None = None,
) -> bool:
    log("단말 Serial 115200 C1DL HELLO 대기")
    log("시리얼 모니터는 닫고 이 프로그램만 사용하세요")
    reset_rx_buf()
    deadline = time.time() + timeout
    last_shown = -1
    while True:
        remain = deadline - time.time()
        remain_i = max(0, int(remain))
        if remain_i != last_shown:
            last_shown = remain_i
            if on_tick is not None:
                on_tick(remain_i)
        if remain <= 0:
            log("HELLO 대기 시간 초과")
            return False
        line = _read_line(ser, min(0.3, remain))
        if not line:
            continue
        log(line)
        if line.startswith("C1DL HELLO") or line.startswith("C1DL READY"):
            _write_cmd(ser, b"C1DL HELLO\n")
            mode_deadline = time.time() + 3.0
            while time.time() < mode_deadline:
                resp = _read_line(ser, 0.3)
                if not resp:
                    continue
                log(resp)
                if resp.startswith("C1DL MODE"):
                    time.sleep(0.4)
                    return True
                if resp.startswith("C1DL HELLO") or resp.startswith("C1DL READY"):
                    _write_cmd(ser, b"C1DL HELLO\n")
            log("HELLO 응답 없음")
            return False
        if line.startswith("C1DL BOOT"):
            log("단말이 이미 정상 부팅으로 넘어갔습니다. 남은 시간 안에 다시 리셋하세요.")


def _list_files_once(
    ser, log: Callable[[str], None], timeout: float
) -> tuple[list[tuple[str, int]], bool]:
    _discard_rx(ser, log, "before LIST")
    _write_cmd(ser, b"C1DL LIST\n")
    files: list[tuple[str, int]] = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = _read_line(ser, 0.2)
        if not line:
            continue
        if line.startswith("C1DL RX"):
            log(line)
            continue
        if line.startswith("C1DL LISTACK"):
            continue
        if line.startswith("C1DL UNK"):
            log(line)
            return files, False
        if line.startswith("C1DL FILE "):
            body = line[10:]
            path, _, size_s = body.rpartition(" ")
            try:
                files.append((path, int(size_s)))
            except ValueError:
                log(f"목록 파싱 실패: {line}")
            continue
        if line.startswith("C1DL LIST END"):
            return files, True
        if line.startswith("C1DL LIST "):
            log(line)
            continue
        log(line)
    return files, False


def list_files(
    ser, log: Callable[[str], None], timeout: float = LIST_TIMEOUT_S
) -> list[tuple[str, int]]:
    files, ok = _list_files_once(ser, log, timeout)
    if not ok:
        log(f"LIST 타임아웃 ({int(timeout)}초)")
    return files


def list_files_retry(
    ser,
    log: Callable[[str], None],
    timeout: float = LIST_TIMEOUT_S,
    tries: int = LIST_TRIES,
) -> list[tuple[str, int]]:
    time.sleep(0.08)
    for i in range(1, tries + 1):
        files, ok = _list_files_once(ser, log, timeout)
        if ok:
            return files
        if i < tries:
            log(f"LIST 실패  재시도 {i}/{tries - 1}")
            time.sleep(0.2)
        else:
            log(f"LIST 실패 ({tries}회)")
    return []


def _is_keepalive(line: str) -> bool:
    return line.startswith(
        (
            "C1DL BUSY",
            "C1DL ERASE",
            "C1DL BEGINACK",
            "C1DL PUTACK",
            "C1DL LISTACK",
            "C1DL RX",
            "C1DL P ",
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
            _write_cmd(ser, line)
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


def _write_all(ser, blob: bytes) -> None:
    """Windows 부하 시 write() 가 일부만 나갈 수 있다. 전부 나갈 때까지 반복."""
    view = memoryview(blob)
    while view:
        n = ser.write(view)
        if not n:
            raise OSError("serial write returned 0")
        view = view[n:]


def _wait_page_ack(ser, log: Callable[[str], None], written: int, timeout: float = 5.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = _read_line(ser, 0.2)
        if not line:
            continue
        if line.startswith("C1DL P "):
            try:
                got = int(line.split()[2])
            except (IndexError, ValueError):
                log(line)
                return False
            if got == written:
                return True
            log(f"페이지 ACK 불일치 {got} != {written}")
            return False
        if line.startswith("C1DL FAIL"):
            log(line)
            return False
        if _is_keepalive(line):
            continue
        log(line)
    log(f"C1DL P {written} 타임아웃")
    return False


def _send_binary(
    ser,
    data: bytes,
    log: Callable[[str], None],
    progress: Callable[[int, int], None] | None = None,
    already: int = 0,
    total: int | None = None,
    page_ack: bool = False,
) -> bool:
    if total is None:
        total = len(data)
    sent = 0
    last_log = -1
    t0 = time.perf_counter()
    page = 256
    while sent < len(data):
        n = min(page, len(data) - sent)
        _write_all(ser, data[sent : sent + n])
        ser.flush()
        sent += n
        if page_ack:
            if not _wait_page_ack(ser, log, sent):
                return False
        elif n == page:
            time.sleep(0.008)
        if progress:
            progress(already + sent, total)
        if sent == len(data) or sent // 16384 != last_log:
            last_log = sent // 16384
            pct = int(sent * 100 / total) if total else 0
            log(f"진행 {pct}%  {sent}/{total}")
    elapsed = time.perf_counter() - t0
    bps = (len(data) / elapsed) if elapsed > 0 else 0
    pending = getattr(ser, "in_waiting", None)
    log(f"바이너리 {len(data)}바이트  {elapsed:.1f}s  {bps:.0f} B/s  RX대기={pending}")
    trace("TX", f"BIN done {len(data)}B {elapsed:.3f}s {bps:.0f}Bps in_waiting={pending}")
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
    _write_cmd(ser, begin)

    if not _wait_ok_line(ser, log, "C1DL GO", 25.0):
        log("GO 없음 (erase 실패 또는 무응답)")
        return False
    log("GO 수신, 바이너리 송신 전 0.35s")
    time.sleep(0.35)
    _discard_rx(ser, log, "after GO")
    trace("TX", f"BIN start {len(pack)}B")
    if not _send_binary(ser, pack, log, progress, page_ack=True):
        return False
    wait_s = max(40.0, len(pack) / 4000.0 + 20.0)
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
    _discard_rx(ser, log, "before PUT")
    _write_cmd(ser, cmd)
    log(f"PUT {path} size={len(data)} crc={total_crc}")

    if not _wait_ok_line(ser, log, "C1DL PUTGO", 5.0):
        return False
    log("PUTGO 수신, 바이너리 송신 전 0.12s")
    time.sleep(0.12)
    _discard_rx(ser, log, "after PUTGO")
    if not _send_binary(
        ser,
        data,
        log,
        progress,
        already,
        total if total is not None else len(data),
    ):
        return False
    wait_s = max(20.0, len(data) / 4000.0 + 10.0)
    return _wait_ok_line(ser, log, "C1DL PUTOK", wait_s)


def quit_session(ser, log: Callable[[str], None]) -> None:
    try:
        _write_cmd(ser, b"C1DL QUIT\n")
        line = _read_line(ser, 1.0)
        if line:
            log(line)
    except Exception as exc:
        log(f"QUIT: {exc}")
