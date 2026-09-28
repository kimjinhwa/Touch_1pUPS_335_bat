# -*- coding: utf-8 -*-
"""C1IM 이미지 팩 — 펌웨어 FlashImagePack 과 동일 포맷."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

MAGIC = b"C1IM"
VERSION = 1
HEADER_SIZE = 16
ENTRY_SIZE = 48
PATH_SIZE = 40
CAPACITY = 0x100000
PAGE = 256


def crc32(data: bytes, value: int = 0) -> int:
    return zlib.crc32(data, value) & 0xFFFFFFFF


def collect_raws(folder: Path) -> list[tuple[str, Path]]:
    folder = folder.resolve()
    if not folder.is_dir():
        raise FileNotFoundError(f"폴더 없음: {folder}")
    root_name = folder.name
    files: list[tuple[str, Path]] = []
    for path in sorted(folder.rglob("*")):
        if not path.is_file():
            continue
        # *.raw.notuse 는 수집하지 않는다.
        if path.suffix.lower() != ".raw":
            continue
        rel = path.relative_to(folder).as_posix()
        stored = f"{root_name}/{rel}"
        stored = stored.rsplit(".", 1)[0] + ".RAW"
        if len(stored.encode("ascii", "replace")) >= PATH_SIZE:
            raise ValueError(f"경로가 너무 깁니다({PATH_SIZE - 1}): {stored}")
        files.append((stored, path))
    return files


def build_pack_from_items(items: list[tuple[str, Path]]) -> tuple[bytes, list[tuple[str, int]]]:
    if not items:
        raise ValueError("*.raw 파일이 없습니다.")

    blobs: list[bytes] = []
    meta: list[tuple[str, int, int]] = []
    n = len(items)
    data_offset = (HEADER_SIZE + n * ENTRY_SIZE + PAGE - 1) // PAGE * PAGE
    cursor = data_offset
    listing: list[tuple[str, int]] = []
    for stored, src in items:
        blob = src.read_bytes()
        blobs.append(blob)
        meta.append((stored, cursor, len(blob)))
        listing.append((stored, len(blob)))
        cursor += len(blob)

    if cursor > CAPACITY:
        raise ValueError(f"팩이 칩 용량을 초과합니다: {cursor} > {CAPACITY}")

    header = bytearray(struct.pack("<4sHHII", MAGIC, VERSION, n, data_offset, 0))
    entries = bytearray()
    for stored, offset, size in meta:
        path_b = stored.encode("ascii")
        path_field = path_b + b"\x00" * (PATH_SIZE - len(path_b))
        entries += path_field + struct.pack("<II", offset, size)

    pad = b"\xFF" * (data_offset - HEADER_SIZE - len(entries))
    pack = bytearray(header + entries + pad + b"".join(blobs))
    struct.pack_into("<I", pack, 12, crc32(bytes(pack)))
    return bytes(pack), listing


def build_pack(folder: Path) -> tuple[bytes, list[tuple[str, int]]]:
    return build_pack_from_items(collect_raws(folder))
