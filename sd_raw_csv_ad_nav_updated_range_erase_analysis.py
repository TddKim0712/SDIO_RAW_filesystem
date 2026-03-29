#!/usr/bin/env python3
from __future__ import annotations

import csv
import ctypes
from ctypes import wintypes
import json
import math
import os
import struct
import subprocess
import time
import tkinter as tk
from collections import Counter
from tkinter import filedialog, messagebox, simpledialog, ttk
from tkinter.scrolledtext import ScrolledText
import zlib

APP_TITLE = "SD Raw-Only Viewer + Analyzer"
DEFAULT_SECTOR_SIZE = 512
LBA_PAGE_STEP = 3
NAV_BINDTAG = "RawViewerNavKeys"
MAX_ERASE_BYTES = 40_000_000_000  # 40 GB decimal

RAW_SUPER_MAGIC_V1 = b"RAWSDIO1"
RAW_SUPER_MAGIC_V2 = b"RAWSDIO2"
RAW_SUPER_MAGIC_V3 = b"RAWSDIO3"
RAW_DATA_MAGIC_V2 = b"RAWDATA1"
RAW_DATA_MAGIC_V3 = b"RAWDATA3"
RAW_TAIL_MAGIC = 0xA55A5AA5
RAW_SENSOR_PACKET_BYTES = 64
RAW_DATA_HEADER_BYTES_V3 = 64
RAW_DATA_PAYLOAD_BYTES_V3 = DEFAULT_SECTOR_SIZE - RAW_DATA_HEADER_BYTES_V3
RAW_LOG_FLAG_PAYLOAD_LINEAR_TEST = 0x00000001
RAW_LOG_FLAG_PAYLOAD_DUMMY_SENSOR = 0x00000002
RAW_LOG_FLAG_PAYLOAD_SENSOR_DMA = 0x00000004
RAW_DUMMY_PACKET_MAGIC = 0x44554D59

GENERIC_READ = 0x80000000
GENERIC_WRITE = 0x40000000
FILE_SHARE_READ = 0x00000001
FILE_SHARE_WRITE = 0x00000002
OPEN_EXISTING = 3
FILE_ATTRIBUTE_NORMAL = 0x00000080
FILE_BEGIN = 0
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value

HEX_EXPORT_FIELDS = [
    "target",
    "sector_size",
    "lba",
    "size",
    "unique_bytes",
    "zeros",
    "first_16",
    "last_16",
    "ascii_preview",
    "raw_hex",
    "hex_dump",
]

PARSED_EXPORT_FIELDS = [
    "target",
    "sector_size",
    "lba",
    "size",
    "kind",
    "kind_detail",
    "magic",
    "version",
    "block_size",
    "card_block_count",
    "superblock_ring_start_lba",
    "superblock_ring_count",
    "data_start_lba",
    "superblock_write_interval",
    "boot_count",
    "write_seq",
    "last_written_lba",
    "next_data_lba",
    "uptime_ms",
    "last_data_write_ms",
    "last_superblock_write_ms",
    "last_total_write_ms",
    "max_total_write_ms",
    "stall_count",
    "seq",
    "declared_lba",
    "tick_ms",
    "data_ms",
    "sb_ms",
    "total_ms",
    "prev_data_ms",
    "prev_superblock_ms",
    "prev_total_ms",
    "cycle_data_ms",
    "cycle_superblock_ms",
    "cycle_total_ms",
    "packet_bytes",
    "packet_count",
    "payload_bytes",
    "payload_crc32",
    "block_crc32",
    "flags",
    "payload_mode",
    "dummy_packet_count",
    "dummy_packet_magic_ok",
    "dummy_packet_magic_ok_count",
    "dummy_packet_first_seq",
    "dummy_packet_last_seq",
    "payload_crc_ok",
    "block_crc_ok",
    "crc_ok",
    "test_pattern_ok",
    "test_pattern_mismatch_count",
    "test_pattern_first_bad_offset",
    "payload_first_16",
    "payload_last_16",
    "tail_magic",
    "unique_bytes",
    "zeros",
    "first_16",
    "last_16",
    "ascii_preview",
    "parsed_text",
]

kernel32 = None
if os.name == "nt":
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)


class RawDeviceError(Exception):
    pass


if os.name == "nt":
    kernel32.CreateFileW.argtypes = [
        wintypes.LPCWSTR,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.LPVOID,
        wintypes.DWORD,
        wintypes.DWORD,
        wintypes.HANDLE,
    ]
    kernel32.CreateFileW.restype = wintypes.HANDLE

    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL

    kernel32.SetFilePointerEx.argtypes = [
        wintypes.HANDLE,
        ctypes.c_longlong,
        ctypes.POINTER(ctypes.c_longlong),
        wintypes.DWORD,
    ]
    kernel32.SetFilePointerEx.restype = wintypes.BOOL

    kernel32.ReadFile.argtypes = [
        wintypes.HANDLE,
        wintypes.LPVOID,
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
        wintypes.LPVOID,
    ]
    kernel32.ReadFile.restype = wintypes.BOOL

    kernel32.WriteFile.argtypes = [
        wintypes.HANDLE,
        wintypes.LPVOID,
        wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD),
        wintypes.LPVOID,
    ]
    kernel32.WriteFile.restype = wintypes.BOOL

    kernel32.FlushFileBuffers.argtypes = [wintypes.HANDLE]
    kernel32.FlushFileBuffers.restype = wintypes.BOOL



def raise_win_error(prefix: str) -> None:
    err = ctypes.get_last_error()
    msg = ctypes.FormatError(err).strip()
    raise RawDeviceError(f"{prefix} / WinError={err}: {msg}")


class RawDeviceReader:
    def __init__(self, path: str):
        self.path = path
        self.handle = None

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def open(self) -> None:
        if os.name != "nt":
            raise RawDeviceError("Windows 전용이다.")
        handle = kernel32.CreateFileW(
            wintypes.LPCWSTR(self.path),
            wintypes.DWORD(GENERIC_READ),
            wintypes.DWORD(FILE_SHARE_READ | FILE_SHARE_WRITE),
            None,
            wintypes.DWORD(OPEN_EXISTING),
            wintypes.DWORD(FILE_ATTRIBUTE_NORMAL),
            None,
        )
        if handle == INVALID_HANDLE_VALUE:
            raise_win_error(f"장치 열기 실패: {self.path}")
        self.handle = handle

    def close(self) -> None:
        if self.handle not in (None, INVALID_HANDLE_VALUE):
            kernel32.CloseHandle(self.handle)
            self.handle = None

    def read_lba(self, lba: int, sector_size: int = DEFAULT_SECTOR_SIZE, count: int = 1) -> bytes:
        if lba < 0:
            raise RawDeviceError("LBA는 0 이상이어야 한다.")
        if sector_size <= 0 or count <= 0:
            raise RawDeviceError("sector_size/count가 잘못되었다.")
        offset = lba * sector_size
        size = sector_size * count
        new_pos = ctypes.c_longlong()
        ok = kernel32.SetFilePointerEx(
            self.handle,
            ctypes.c_longlong(offset),
            ctypes.byref(new_pos),
            wintypes.DWORD(FILE_BEGIN),
        )
        if not ok:
            raise_win_error(f"seek 실패: offset={offset}")
        buf = ctypes.create_string_buffer(size)
        read = wintypes.DWORD(0)
        ok = kernel32.ReadFile(
            self.handle,
            buf,
            wintypes.DWORD(size),
            ctypes.byref(read),
            None,
        )
        if not ok:
            raise_win_error(f"read 실패: offset={offset}, size={size}")
        return buf.raw[:read.value]


class RawDeviceWriter:
    def __init__(self, path: str):
        self.path = path
        self.handle = None

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def open(self) -> None:
        if os.name != "nt":
            raise RawDeviceError("Windows 전용이다.")
        handle = kernel32.CreateFileW(
            wintypes.LPCWSTR(self.path),
            wintypes.DWORD(GENERIC_READ | GENERIC_WRITE),
            wintypes.DWORD(FILE_SHARE_READ | FILE_SHARE_WRITE),
            None,
            wintypes.DWORD(OPEN_EXISTING),
            wintypes.DWORD(FILE_ATTRIBUTE_NORMAL),
            None,
        )
        if handle == INVALID_HANDLE_VALUE:
            raise_win_error(f"장치 열기 실패: {self.path}")
        self.handle = handle

    def close(self) -> None:
        if self.handle not in (None, INVALID_HANDLE_VALUE):
            kernel32.CloseHandle(self.handle)
            self.handle = None

    def flush(self) -> None:
        if self.handle not in (None, INVALID_HANDLE_VALUE):
            ok = kernel32.FlushFileBuffers(self.handle)
            if not ok:
                raise_win_error("FlushFileBuffers 실패")

    def zero_fill_lba_range(
        self,
        start_lba: int,
        end_lba: int,
        sector_size: int,
        chunk_lbas: int,
        progress_cb=None,
    ) -> None:
        if start_lba < 0 or end_lba < start_lba:
            raise RawDeviceError("LBA 범위가 잘못되었다.")
        if sector_size <= 0:
            raise RawDeviceError("sector_size가 잘못되었다.")
        if chunk_lbas <= 0:
            raise RawDeviceError("chunk_lbas가 잘못되었다.")

        total_lbas = end_lba - start_lba + 1
        zero_buf = ctypes.create_string_buffer(sector_size * chunk_lbas)
        done = 0
        current = start_lba
        t0 = time.time()

        while current <= end_lba:
            this_lbas = min(chunk_lbas, end_lba - current + 1)
            this_bytes = sector_size * this_lbas
            offset = current * sector_size

            new_pos = ctypes.c_longlong()
            ok = kernel32.SetFilePointerEx(
                self.handle,
                ctypes.c_longlong(offset),
                ctypes.byref(new_pos),
                wintypes.DWORD(FILE_BEGIN),
            )
            if not ok:
                raise_win_error(f"seek 실패: lba={current}, offset={offset}")

            written = wintypes.DWORD(0)
            ok = kernel32.WriteFile(
                self.handle,
                zero_buf,
                wintypes.DWORD(this_bytes),
                ctypes.byref(written),
                None,
            )
            if not ok:
                raise_win_error(f"write 실패: lba={current}, bytes={this_bytes}")
            if written.value != this_bytes:
                raise RawDeviceError(
                    f"short write: lba={current}, expected={this_bytes}, actual={written.value}"
                )

            done += this_lbas
            current += this_lbas

            if progress_cb is not None:
                elapsed = max(time.time() - t0, 1e-6)
                mib_per_sec = (done * sector_size) / elapsed / (1024 * 1024)
                progress_cb(done, total_lbas, current - 1, mib_per_sec)

        self.flush()



def ps_json(script: str):
    cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script]
    cp = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if cp.returncode != 0:
        raise RuntimeError(cp.stderr.strip() or cp.stdout.strip() or "PowerShell 실행 실패")
    out = cp.stdout.strip()
    if not out:
        return []
    data = json.loads(out)
    return data if isinstance(data, list) else [data]



def scan_physical_drives():
    disks = ps_json(
        r'''
Get-Disk |
Select-Object Number, FriendlyName, BusType, Size, PartitionStyle,
              IsBoot, IsSystem, IsOffline, IsReadOnly,
              LogicalSectorSize, PhysicalSectorSize |
ConvertTo-Json -Compress
'''
    )
    result = []
    for d in disks:
        number = int(d.get("Number", 0))
        size = int(d.get("Size", 0) or 0)
        bus = str(d.get("BusType") or "")
        name = str(d.get("FriendlyName") or f"Disk {number}")
        is_boot = bool(d.get("IsBoot"))
        is_system = bool(d.get("IsSystem"))
        is_offline = bool(d.get("IsOffline"))
        is_readonly = bool(d.get("IsReadOnly"))
        logical_sector_size = int(d.get("LogicalSectorSize", DEFAULT_SECTOR_SIZE) or DEFAULT_SECTOR_SIZE)
        physical_sector_size = int(d.get("PhysicalSectorSize", logical_sector_size) or logical_sector_size)

        score = 0
        if not is_boot and not is_system:
            score += 50
        if 24 * 1024**3 <= size <= 40 * 1024**3:
            score += 20
        if bus.upper() in ("USB", "SD", "MMC", "SCSI"):
            score += 10

        result.append({
            "target": rf"\\.\PhysicalDrive{number}",
            "number": number,
            "name": name,
            "bus": bus,
            "size": size,
            "is_boot": is_boot,
            "is_system": is_system,
            "is_offline": is_offline,
            "is_readonly": is_readonly,
            "logical_sector_size": logical_sector_size,
            "physical_sector_size": physical_sector_size,
            "score": score,
        })

    result.sort(key=lambda item: (item["score"], item["size"]), reverse=True)
    return result



def size_text(size: int) -> str:
    if size <= 0:
        return "?"
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    value = float(size)
    idx = 0
    while value >= 1024.0 and idx < len(units) - 1:
        value /= 1024.0
        idx += 1
    return f"{value:.1f} {units[idx]}"



def decimal_gb_text(size: int) -> str:
    return f"{size / 1_000_000_000:.2f} GB"



def hexdump(data: bytes, width: int = 16) -> str:
    lines = []
    for off in range(0, len(data), width):
        chunk = data[off:off + width]
        hex_part = " ".join(f"{b:02X}" for b in chunk)
        ascii_part = "".join(chr(b) if 32 <= b <= 126 else "." for b in chunk)
        lines.append(f"{off:04X}  {hex_part:<{width*3}}  {ascii_part}")
    return "\n".join(lines)



def compute_crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF



def u32_delta(curr: int, prev: int) -> int:
    return (curr - prev) & 0xFFFFFFFF



def expected_test_byte(seq: int, lba: int, offset: int) -> int:
    return (seq + lba + offset) & 0xFF



def count_test_pattern_mismatches(payload: bytes, seq: int, lba: int, payload_bytes: int) -> tuple[int, int | None]:
    mismatch_count = 0
    first_bad = None
    compare_bytes = min(len(payload), max(0, payload_bytes))

    for idx in range(compare_bytes):
        if payload[idx] != expected_test_byte(seq, lba, idx):
            mismatch_count += 1
            if first_bad is None:
                first_bad = idx

    return mismatch_count, first_bad



def parse_dummy_packets(payload: bytes, payload_bytes: int, packet_bytes: int) -> dict:
    usable_bytes = min(len(payload), max(0, payload_bytes))
    full_packet_bytes = 0
    packet_count = 0
    magic_ok_count = 0
    first_seq = None
    last_seq = None

    if packet_bytes <= 0:
        return {
            "dummy_packet_count": 0,
            "dummy_packet_magic_ok": None,
            "dummy_packet_magic_ok_count": 0,
            "dummy_packet_first_seq": "",
            "dummy_packet_last_seq": "",
        }

    full_packet_bytes = usable_bytes - (usable_bytes % packet_bytes)
    packet_count = full_packet_bytes // packet_bytes

    for packet_idx in range(packet_count):
        off = packet_idx * packet_bytes
        packet = payload[off:off + packet_bytes]
        if len(packet) < 8:
            continue

        magic, packet_seq = struct.unpack_from("<2I", packet, 0)
        if magic == RAW_DUMMY_PACKET_MAGIC:
            magic_ok_count += 1
        if first_seq is None:
            first_seq = packet_seq
        last_seq = packet_seq

    return {
        "dummy_packet_count": packet_count,
        "dummy_packet_magic_ok": (None if packet_count == 0 else (magic_ok_count == packet_count)),
        "dummy_packet_magic_ok_count": magic_ok_count,
        "dummy_packet_first_seq": "" if first_seq is None else first_seq,
        "dummy_packet_last_seq": "" if last_seq is None else last_seq,
    }



def detect_payload_mode(flags: int,
                        packet_bytes: int,
                        payload: bytes,
                        payload_bytes: int,
                        mismatch_count: int,
                        dummy_info: dict) -> str:
    if (flags & RAW_LOG_FLAG_PAYLOAD_DUMMY_SENSOR) != 0:
        return "dummy_packets"
    if (flags & RAW_LOG_FLAG_PAYLOAD_LINEAR_TEST) != 0:
        return "linear_test"
    if (flags & RAW_LOG_FLAG_PAYLOAD_SENSOR_DMA) != 0:
        return "sensor_dma"

    if (
        packet_bytes == RAW_SENSOR_PACKET_BYTES
        and dummy_info.get("dummy_packet_count", 0) > 0
        and dummy_info.get("dummy_packet_magic_ok") is True
    ):
        return "dummy_packets(auto)"

    if payload_bytes > 0 and mismatch_count == 0:
        return "linear_test(auto)"

    return "unknown"



def pct_text(numerator: int, denominator: int) -> str:
    if denominator <= 0:
        return "n/a"
    return f"{(100.0 * numerator / denominator):.2f}%"



def fmt_hex32(value: int | None) -> str:
    if value is None:
        return ""
    return f"0x{value & 0xFFFFFFFF:08X}"



def safe_int(value, default=None):
    try:
        if value in (None, ""):
            return default
        return int(value)
    except Exception:
        return default



def percentile(values: list[int], p: float):
    if not values:
        return None
    ordered = sorted(values)
    idx = max(0, min(len(ordered) - 1, math.ceil((p / 100.0) * len(ordered)) - 1))
    return ordered[idx]



def summarize_ms(values: list[int]) -> str:
    if not values:
        return "n/a"
    avg = sum(values) / len(values)
    p50 = percentile(values, 50)
    p95 = percentile(values, 95)
    return (
        f"n={len(values)}, min={min(values)}, avg={avg:.2f}, "
        f"p50={p50}, p95={p95}, max={max(values)}"
    )



def read_block_crc32_with_zeroed_field(data: bytes, offset: int) -> int:
    temp = bytearray(data)
    temp[offset:offset + 4] = b"\x00\x00\x00\x00"
    return compute_crc32(bytes(temp))



def parse_superblock_v1_info(data: bytes) -> dict | None:
    if len(data) != DEFAULT_SECTOR_SIZE or data[:8] != RAW_SUPER_MAGIC_V1:
        return None

    version, block_size, card_blocks, data_start, boot_count, pat_start, pat_count = struct.unpack_from("<7I", data, 8)
    tail_magic, = struct.unpack_from("<I", data, 508)

    return {
        "kind": "superblock_v1",
        "kind_detail": "superblock_v1",
        "magic": data[:8].decode("ascii", errors="replace"),
        "version": version,
        "block_size": block_size,
        "card_block_count": card_blocks,
        "data_start_lba": data_start,
        "boot_count": boot_count,
        "pattern_start_lba": pat_start,
        "pattern_block_count": pat_count,
        "tail_magic": fmt_hex32(tail_magic),
    }



def parse_superblock_v2_info(data: bytes) -> dict | None:
    if len(data) != DEFAULT_SECTOR_SIZE or data[:8] != RAW_SUPER_MAGIC_V2:
        return None

    (
        version,
        block_size,
        card_block_count,
        superblock_ring_start_lba,
        superblock_ring_count,
        data_start_lba,
        boot_count,
        write_seq,
        last_written_lba,
        next_data_lba,
        uptime_ms,
        last_data_write_ms,
        last_total_write_ms,
        max_total_write_ms,
        stall_count,
    ) = struct.unpack_from("<15I", data, 8)
    tail_magic, = struct.unpack_from("<I", data, 508)

    return {
        "kind": "superblock_v2",
        "kind_detail": "superblock_v2",
        "magic": data[:8].decode("ascii", errors="replace"),
        "version": version,
        "block_size": block_size,
        "card_block_count": card_block_count,
        "superblock_ring_start_lba": superblock_ring_start_lba,
        "superblock_ring_count": superblock_ring_count,
        "data_start_lba": data_start_lba,
        "boot_count": boot_count,
        "write_seq": write_seq,
        "last_written_lba": last_written_lba,
        "next_data_lba": next_data_lba,
        "uptime_ms": uptime_ms,
        "last_data_write_ms": last_data_write_ms,
        "last_total_write_ms": last_total_write_ms,
        "max_total_write_ms": max_total_write_ms,
        "stall_count": stall_count,
        "tail_magic": fmt_hex32(tail_magic),
    }



def parse_superblock_v3_info(data: bytes) -> dict | None:
    if len(data) != DEFAULT_SECTOR_SIZE or data[:8] != RAW_SUPER_MAGIC_V3:
        return None

    values = struct.unpack_from("<18I", data, 8)
    (
        version,
        block_size,
        card_block_count,
        superblock_ring_start_lba,
        superblock_ring_count,
        data_start_lba,
        superblock_write_interval,
        boot_count,
        write_seq,
        last_written_lba,
        next_data_lba,
        uptime_ms,
        last_data_write_ms,
        last_superblock_write_ms,
        last_total_write_ms,
        max_total_write_ms,
        stall_count,
        block_crc32,
    ) = values
    tail_magic, = struct.unpack_from("<I", data, 508)
    block_crc_calc = read_block_crc32_with_zeroed_field(data, 76)
    block_crc_ok = (block_crc32 == block_crc_calc)

    return {
        "kind": "superblock_v3",
        "kind_detail": "superblock_v3",
        "magic": data[:8].decode("ascii", errors="replace"),
        "version": version,
        "block_size": block_size,
        "card_block_count": card_block_count,
        "superblock_ring_start_lba": superblock_ring_start_lba,
        "superblock_ring_count": superblock_ring_count,
        "data_start_lba": data_start_lba,
        "superblock_write_interval": superblock_write_interval,
        "boot_count": boot_count,
        "write_seq": write_seq,
        "last_written_lba": last_written_lba,
        "next_data_lba": next_data_lba,
        "uptime_ms": uptime_ms,
        "last_data_write_ms": last_data_write_ms,
        "last_superblock_write_ms": last_superblock_write_ms,
        "last_total_write_ms": last_total_write_ms,
        "max_total_write_ms": max_total_write_ms,
        "stall_count": stall_count,
        "block_crc32": fmt_hex32(block_crc32),
        "block_crc_ok": block_crc_ok,
        "crc_ok": block_crc_ok,
        "tail_magic": fmt_hex32(tail_magic),
    }



def parse_data_block_v2_info(data: bytes) -> dict | None:
    if len(data) != DEFAULT_SECTOR_SIZE or data[:8] != RAW_DATA_MAGIC_V2:
        return None

    version, seq, declared_lba, boot_count, tick_ms, data_ms, sb_ms, total_ms = struct.unpack_from("<8I", data, 8)
    payload = data[40:]
    mismatch_count, first_bad = count_test_pattern_mismatches(payload, seq, declared_lba, len(payload))

    return {
        "kind": "data_block",
        "kind_detail": "data_block_v2",
        "magic": data[:8].decode("ascii", errors="replace"),
        "version": version,
        "seq": seq,
        "declared_lba": declared_lba,
        "boot_count": boot_count,
        "tick_ms": tick_ms,
        "data_ms": data_ms,
        "sb_ms": sb_ms,
        "total_ms": total_ms,
        "cycle_data_ms": data_ms,
        "cycle_superblock_ms": sb_ms,
        "cycle_total_ms": total_ms,
        "payload_bytes": len(payload),
        "payload_mode": "linear_test(v2)",
        "dummy_packet_count": "",
        "dummy_packet_magic_ok": "",
        "dummy_packet_magic_ok_count": "",
        "dummy_packet_first_seq": "",
        "dummy_packet_last_seq": "",
        "payload_first_16": payload[:16].hex(" "),
        "payload_last_16": payload[-16:].hex(" ") if payload else "",
        "test_pattern_ok": (mismatch_count == 0),
        "test_pattern_mismatch_count": mismatch_count,
        "test_pattern_first_bad_offset": "" if first_bad is None else first_bad,
    }



def parse_data_block_v3_info(data: bytes) -> dict | None:
    if len(data) != DEFAULT_SECTOR_SIZE or data[:8] != RAW_DATA_MAGIC_V3:
        return None

    (
        version,
        seq,
        declared_lba,
        boot_count,
        tick_ms,
        prev_data_ms,
        prev_superblock_ms,
        prev_total_ms,
        packet_bytes,
        packet_count,
        payload_bytes,
        payload_crc32,
        block_crc32,
        flags,
    ) = struct.unpack_from("<14I", data, 8)

    payload = data[64:]
    usable_payload = payload[:min(len(payload), max(0, payload_bytes))]
    payload_crc_calc = compute_crc32(usable_payload)
    block_crc_calc = read_block_crc32_with_zeroed_field(data, 56)
    payload_crc_ok = (payload_bytes <= len(payload)) and (payload_crc32 == payload_crc_calc)
    block_crc_ok = (block_crc32 == block_crc_calc)
    mismatch_count, first_bad = count_test_pattern_mismatches(payload, seq, declared_lba, payload_bytes)
    dummy_info = parse_dummy_packets(usable_payload, payload_bytes, packet_bytes)
    payload_mode = detect_payload_mode(flags,
                                       packet_bytes,
                                       usable_payload,
                                       payload_bytes,
                                       mismatch_count,
                                       dummy_info)

    if payload_mode.startswith("linear_test"):
        test_pattern_ok = (mismatch_count == 0)
        test_pattern_mismatch_count = mismatch_count
        test_pattern_first_bad_offset = "" if first_bad is None else first_bad
    else:
        test_pattern_ok = ""
        test_pattern_mismatch_count = ""
        test_pattern_first_bad_offset = ""

    return {
        "kind": "data_block_v3",
        "kind_detail": "data_block_v3",
        "magic": data[:8].decode("ascii", errors="replace"),
        "version": version,
        "seq": seq,
        "declared_lba": declared_lba,
        "boot_count": boot_count,
        "tick_ms": tick_ms,
        "data_ms": prev_data_ms,
        "sb_ms": prev_superblock_ms,
        "total_ms": prev_total_ms,
        "prev_data_ms": prev_data_ms,
        "prev_superblock_ms": prev_superblock_ms,
        "prev_total_ms": prev_total_ms,
        "cycle_data_ms": prev_data_ms,
        "cycle_superblock_ms": prev_superblock_ms,
        "cycle_total_ms": prev_total_ms,
        "packet_bytes": packet_bytes,
        "packet_count": packet_count,
        "payload_bytes": payload_bytes,
        "payload_crc32": fmt_hex32(payload_crc32),
        "block_crc32": fmt_hex32(block_crc32),
        "flags": flags,
        "payload_mode": payload_mode,
        "dummy_packet_count": dummy_info.get("dummy_packet_count", 0),
        "dummy_packet_magic_ok": dummy_info.get("dummy_packet_magic_ok"),
        "dummy_packet_magic_ok_count": dummy_info.get("dummy_packet_magic_ok_count", 0),
        "dummy_packet_first_seq": dummy_info.get("dummy_packet_first_seq", ""),
        "dummy_packet_last_seq": dummy_info.get("dummy_packet_last_seq", ""),
        "payload_crc_ok": payload_crc_ok,
        "block_crc_ok": block_crc_ok,
        "crc_ok": payload_crc_ok and block_crc_ok,
        "payload_first_16": usable_payload[:16].hex(" "),
        "payload_last_16": usable_payload[-16:].hex(" ") if usable_payload else "",
        "test_pattern_ok": test_pattern_ok,
        "test_pattern_mismatch_count": test_pattern_mismatch_count,
        "test_pattern_first_bad_offset": test_pattern_first_bad_offset,
    }


def parse_sector_info(lba: int, data: bytes) -> dict:
    info = {
        "lba": lba,
        "size": len(data),
        "unique_bytes": len(set(data)),
        "zeros": data.count(0),
        "first_16": data[:16].hex(" "),
        "last_16": data[-16:].hex(" "),
        "ascii_preview": "".join(chr(b) if 32 <= b <= 126 else "." for b in data[:64]),
        "kind": "unknown",
        "kind_detail": "unknown",
        "raw_hex": data.hex(" "),
    }

    parsed = parse_superblock_v3_info(data)
    if parsed is None:
        parsed = parse_superblock_v2_info(data)
    if parsed is None:
        parsed = parse_superblock_v1_info(data)
    if parsed is None:
        parsed = parse_data_block_v3_info(data)
    if parsed is None:
        parsed = parse_data_block_v2_info(data)
    if parsed is not None:
        info.update(parsed)

    return info



def format_parsed_info(info: dict) -> str:
    lines = [
        f"LBA {info['lba']}",
        f"  size={info['size']} bytes",
        f"  unique_bytes={info['unique_bytes']}",
        f"  zeros={info['zeros']}",
        f"  first_16={info['first_16']}",
        f"  last_16={info['last_16']}",
        f"  ascii_preview='{info['ascii_preview']}'",
    ]

    kind = info.get("kind")
    if kind == "superblock_v1":
        lines.extend([
            "RAW superblock detected (v1)",
            f"  magic={info.get('magic', '')}",
            f"  version={info.get('version', '')}",
            f"  block_size={info.get('block_size', '')}",
            f"  card_block_count={info.get('card_block_count', '')}",
            f"  data_start_lba={info.get('data_start_lba', '')}",
            f"  boot_count={info.get('boot_count', '')}",
            f"  pattern_start_lba={info.get('pattern_start_lba', '')}",
            f"  pattern_block_count={info.get('pattern_block_count', '')}",
            f"  tail_magic={info.get('tail_magic', '')}",
        ])
    elif kind == "superblock_v2":
        lines.extend([
            "RAW superblock detected (v2)",
            f"  magic={info.get('magic', '')}",
            f"  version={info.get('version', '')}",
            f"  block_size={info.get('block_size', '')}",
            f"  card_block_count={info.get('card_block_count', '')}",
            f"  superblock_ring_start_lba={info.get('superblock_ring_start_lba', '')}",
            f"  superblock_ring_count={info.get('superblock_ring_count', '')}",
            f"  data_start_lba={info.get('data_start_lba', '')}",
            f"  boot_count={info.get('boot_count', '')}",
            f"  write_seq={info.get('write_seq', '')}",
            f"  last_written_lba={info.get('last_written_lba', '')}",
            f"  next_data_lba={info.get('next_data_lba', '')}",
            f"  uptime_ms={info.get('uptime_ms', '')}",
            f"  last_data_write_ms={info.get('last_data_write_ms', '')}",
            f"  last_total_write_ms={info.get('last_total_write_ms', '')}",
            f"  max_total_write_ms={info.get('max_total_write_ms', '')}",
            f"  stall_count={info.get('stall_count', '')}",
            f"  tail_magic={info.get('tail_magic', '')}",
        ])
    elif kind == "superblock_v3":
        lines.extend([
            "RAW superblock detected (v3)",
            f"  magic={info.get('magic', '')}",
            f"  version={info.get('version', '')}",
            f"  block_size={info.get('block_size', '')}",
            f"  card_block_count={info.get('card_block_count', '')}",
            f"  superblock_ring_start_lba={info.get('superblock_ring_start_lba', '')}",
            f"  superblock_ring_count={info.get('superblock_ring_count', '')}",
            f"  data_start_lba={info.get('data_start_lba', '')}",
            f"  superblock_write_interval={info.get('superblock_write_interval', '')}",
            f"  boot_count={info.get('boot_count', '')}",
            f"  write_seq={info.get('write_seq', '')}",
            f"  last_written_lba={info.get('last_written_lba', '')}",
            f"  next_data_lba={info.get('next_data_lba', '')}",
            f"  uptime_ms={info.get('uptime_ms', '')}",
            f"  last_data_write_ms={info.get('last_data_write_ms', '')}",
            f"  last_superblock_write_ms={info.get('last_superblock_write_ms', '')}",
            f"  last_total_write_ms={info.get('last_total_write_ms', '')}",
            f"  max_total_write_ms={info.get('max_total_write_ms', '')}",
            f"  stall_count={info.get('stall_count', '')}",
            f"  block_crc32={info.get('block_crc32', '')}",
            f"  block_crc_ok={info.get('block_crc_ok', '')}",
            f"  tail_magic={info.get('tail_magic', '')}",
        ])
    elif kind == "data_block":
        lines.extend([
            "RAW data block detected (v2)",
            f"  magic={info.get('magic', '')}",
            f"  version={info.get('version', '')}",
            f"  seq={info.get('seq', '')}",
            f"  declared_lba={info.get('declared_lba', '')}",
            f"  boot_count={info.get('boot_count', '')}",
            f"  tick_ms={info.get('tick_ms', '')}",
            f"  data_ms={info.get('data_ms', '')}",
            f"  sb_ms={info.get('sb_ms', '')}",
            f"  total_ms={info.get('total_ms', '')}",
            f"  payload_first_16={info.get('payload_first_16', '')}",
            f"  payload_last_16={info.get('payload_last_16', '')}",
            f"  test_pattern_ok={info.get('test_pattern_ok', '')}",
            f"  test_pattern_mismatch_count={info.get('test_pattern_mismatch_count', '')}",
            f"  test_pattern_first_bad_offset={info.get('test_pattern_first_bad_offset', '')}",
        ])
    elif kind == "data_block_v3":
        lines.extend([
            "RAW data block detected (v3)",
            f"  magic={info.get('magic', '')}",
            f"  version={info.get('version', '')}",
            f"  seq={info.get('seq', '')}",
            f"  declared_lba={info.get('declared_lba', '')}",
            f"  boot_count={info.get('boot_count', '')}",
            f"  tick_ms={info.get('tick_ms', '')}",
            f"  prev_data_ms={info.get('prev_data_ms', '')}",
            f"  prev_superblock_ms={info.get('prev_superblock_ms', '')}",
            f"  prev_total_ms={info.get('prev_total_ms', '')}",
            f"  packet_bytes={info.get('packet_bytes', '')}",
            f"  packet_count={info.get('packet_count', '')}",
            f"  payload_bytes={info.get('payload_bytes', '')}",
            f"  payload_crc32={info.get('payload_crc32', '')}",
            f"  block_crc32={info.get('block_crc32', '')}",
            f"  payload_crc_ok={info.get('payload_crc_ok', '')}",
            f"  block_crc_ok={info.get('block_crc_ok', '')}",
            f"  crc_ok={info.get('crc_ok', '')}",
            f"  flags={info.get('flags', '')}",
            f"  payload_mode={info.get('payload_mode', '')}",
            f"  dummy_packet_count={info.get('dummy_packet_count', '')}",
            f"  dummy_packet_magic_ok={info.get('dummy_packet_magic_ok', '')}",
            f"  dummy_packet_magic_ok_count={info.get('dummy_packet_magic_ok_count', '')}",
            f"  dummy_packet_first_seq={info.get('dummy_packet_first_seq', '')}",
            f"  dummy_packet_last_seq={info.get('dummy_packet_last_seq', '')}",
            f"  payload_first_16={info.get('payload_first_16', '')}",
            f"  payload_last_16={info.get('payload_last_16', '')}",
            f"  test_pattern_ok={info.get('test_pattern_ok', '')}",
            f"  test_pattern_mismatch_count={info.get('test_pattern_mismatch_count', '')}",
            f"  test_pattern_first_bad_offset={info.get('test_pattern_first_bad_offset', '')}",
        ])

    return "\n".join(lines)


def device_flags_text(drive: dict) -> str:
    flags = []
    if drive.get("is_boot"):
        flags.append("BOOT")
    if drive.get("is_system"):
        flags.append("SYSTEM")
    if drive.get("is_offline"):
        flags.append("OFFLINE")
    if drive.get("is_readonly"):
        flags.append("READONLY")
    return ",".join(flags) if flags else "-"



def is_data_kind(kind: str) -> bool:
    return kind in ("data_block", "data_block_v3")



def is_super_kind(kind: str) -> bool:
    return kind.startswith("superblock_")



def make_event(event_type: str, lba: int, seq=None, boot=None, value=None, detail: str = "") -> dict:
    return {
        "type": event_type,
        "lba": lba,
        "seq": "" if seq is None else seq,
        "boot": "" if boot is None else boot,
        "value": "" if value is None else value,
        "detail": detail,
    }



def analyze_infos(infos: list[dict], stall_threshold_ms: int, assume_dummy_pattern: bool) -> tuple[str, list[dict]]:
    force_linear_pattern_check = assume_dummy_pattern

    events: list[dict] = []
    counts = Counter(info.get("kind", "unknown") for info in infos)
    data_infos = [info for info in infos if is_data_kind(info.get("kind", ""))]
    super_infos = [info for info in infos if is_super_kind(info.get("kind", ""))]
    unknown_infos = [info for info in infos if info.get("kind") == "unknown"]

    payload_crc_fail = 0
    block_crc_fail = 0
    linear_pattern_fail = 0
    dummy_magic_fail = 0
    declared_lba_mismatch = 0
    seq_gap_count = 0
    missing_seq_blocks = 0
    duplicate_seq_count = 0
    backward_seq_count = 0
    actual_lba_gap_count = 0
    boot_change_count = 0

    dummy_mode_count = 0
    linear_mode_count = 0
    sensor_dma_mode_count = 0
    unknown_payload_mode_count = 0

    tick_deltas: list[int] = []
    cycle_data_ms_values: list[int] = []
    cycle_super_ms_values: list[int] = []
    cycle_total_ms_values: list[int] = []
    embedded_stall_count = 0
    tick_stall_count = 0

    latest_super = None
    for sb in super_infos:
        if latest_super is None:
            latest_super = sb
        elif safe_int(sb.get("write_seq"), -1) > safe_int(latest_super.get("write_seq"), -1):
            latest_super = sb

    data_start_lba = safe_int(latest_super.get("data_start_lba")) if latest_super is not None else None
    next_data_lba = safe_int(latest_super.get("next_data_lba")) if latest_super is not None else None

    unknown_before_next_data_lba = 0
    unknown_after_next_data_lba = 0

    for info in unknown_infos:
        lba = info["lba"]
        detail = f"first16={info['first_16']}"

        if (data_start_lba is not None) and (next_data_lba is not None):
            if data_start_lba <= lba < next_data_lba:
                unknown_before_next_data_lba += 1
                detail += " / before next_data_lba"
                events.append(make_event("UNKNOWN_IN_WRITTEN_WINDOW", lba, value=info.get("unique_bytes"), detail=detail))
            elif lba >= next_data_lba:
                unknown_after_next_data_lba += 1
        elif (data_start_lba is not None) and (lba >= data_start_lba):
            detail += " / data area"
            events.append(make_event("UNKNOWN", lba, value=info.get("unique_bytes"), detail=detail))

    previous_data = None
    for info in data_infos:
        seq = safe_int(info.get("seq"))
        boot = safe_int(info.get("boot_count"))
        declared_lba = safe_int(info.get("declared_lba"))
        tick_ms = safe_int(info.get("tick_ms"), 0)
        cycle_total_ms = safe_int(info.get("cycle_total_ms"))
        cycle_data_ms = safe_int(info.get("cycle_data_ms"))
        cycle_super_ms = safe_int(info.get("cycle_superblock_ms"))
        payload_mode = str(info.get("payload_mode") or "")

        if payload_mode.startswith("dummy_packets"):
            dummy_mode_count += 1
        elif payload_mode.startswith("linear_test"):
            linear_mode_count += 1
        elif payload_mode == "sensor_dma":
            sensor_dma_mode_count += 1
        else:
            unknown_payload_mode_count += 1

        if cycle_data_ms is not None:
            cycle_data_ms_values.append(cycle_data_ms)
        if cycle_super_ms is not None:
            cycle_super_ms_values.append(cycle_super_ms)
        if cycle_total_ms is not None:
            cycle_total_ms_values.append(cycle_total_ms)
            if cycle_total_ms >= stall_threshold_ms:
                embedded_stall_count += 1
                events.append(make_event(
                    "CYCLE_STALL",
                    info["lba"],
                    seq=seq,
                    boot=boot,
                    value=cycle_total_ms,
                    detail=f"cycle_total_ms={cycle_total_ms}",
                ))

        if declared_lba is not None and declared_lba != info["lba"]:
            declared_lba_mismatch += 1
            events.append(make_event(
                "DECLARED_LBA_MISMATCH",
                info["lba"],
                seq=seq,
                boot=boot,
                value=declared_lba,
                detail=f"declared_lba={declared_lba}, actual_lba={info['lba']}",
            ))

        if info.get("kind") == "data_block_v3":
            if info.get("payload_crc_ok") is False:
                payload_crc_fail += 1
                events.append(make_event(
                    "PAYLOAD_CRC_FAIL",
                    info["lba"],
                    seq=seq,
                    boot=boot,
                    detail=f"payload_crc32={info.get('payload_crc32', '')}",
                ))
            if info.get("block_crc_ok") is False:
                block_crc_fail += 1
                events.append(make_event(
                    "BLOCK_CRC_FAIL",
                    info["lba"],
                    seq=seq,
                    boot=boot,
                    detail=f"block_crc32={info.get('block_crc32', '')}",
                ))

            if payload_mode.startswith("dummy_packets"):
                if info.get("dummy_packet_magic_ok") is False:
                    dummy_magic_fail += 1
                    events.append(make_event(
                        "DUMMY_PACKET_MAGIC_FAIL",
                        info["lba"],
                        seq=seq,
                        boot=boot,
                        value=info.get("dummy_packet_magic_ok_count"),
                        detail=(
                            f"magic_ok={info.get('dummy_packet_magic_ok_count', '')}/"
                            f"{info.get('dummy_packet_count', '')}"
                        ),
                    ))
            elif payload_mode.startswith("linear_test"):
                if info.get("test_pattern_ok") is False:
                    linear_pattern_fail += 1
                    events.append(make_event(
                        "LINEAR_PATTERN_FAIL",
                        info["lba"],
                        seq=seq,
                        boot=boot,
                        value=info.get("test_pattern_mismatch_count"),
                        detail=(
                            f"mismatch={info.get('test_pattern_mismatch_count', '')}, "
                            f"first_bad={info.get('test_pattern_first_bad_offset', '')}"
                        ),
                    ))
            elif force_linear_pattern_check and info.get("test_pattern_ok") is False:
                linear_pattern_fail += 1
                events.append(make_event(
                    "LEGACY_LINEAR_PATTERN_FAIL",
                    info["lba"],
                    seq=seq,
                    boot=boot,
                    value=info.get("test_pattern_mismatch_count"),
                    detail=(
                        f"mismatch={info.get('test_pattern_mismatch_count', '')}, "
                        f"first_bad={info.get('test_pattern_first_bad_offset', '')}"
                    ),
                ))

        if previous_data is not None:
            prev_seq = safe_int(previous_data.get("seq"))
            prev_boot = safe_int(previous_data.get("boot_count"))
            prev_tick = safe_int(previous_data.get("tick_ms"), 0)

            if boot != prev_boot:
                boot_change_count += 1
                events.append(make_event(
                    "BOOT_CHANGE",
                    info["lba"],
                    seq=seq,
                    boot=boot,
                    detail=f"boot_count {prev_boot} -> {boot}",
                ))
            else:
                if prev_seq is not None and seq is not None:
                    if seq == prev_seq:
                        duplicate_seq_count += 1
                        events.append(make_event(
                            "SEQ_DUP",
                            info["lba"],
                            seq=seq,
                            boot=boot,
                            detail=f"prev_seq={prev_seq}",
                        ))
                    elif seq > prev_seq:
                        diff = seq - prev_seq
                        if diff > 1:
                            seq_gap_count += 1
                            missing_seq_blocks += (diff - 1)
                            events.append(make_event(
                                "SEQ_GAP",
                                info["lba"],
                                seq=seq,
                                boot=boot,
                                value=diff - 1,
                                detail=f"prev_seq={prev_seq}, curr_seq={seq}",
                            ))
                    else:
                        backward_seq_count += 1
                        events.append(make_event(
                            "SEQ_BACKWARD",
                            info["lba"],
                            seq=seq,
                            boot=boot,
                            detail=f"prev_seq={prev_seq}, curr_seq={seq}",
                        ))

                if info["lba"] != previous_data["lba"] + 1:
                    actual_lba_gap_count += 1
                    events.append(make_event(
                        "LBA_GAP",
                        info["lba"],
                        seq=seq,
                        boot=boot,
                        value=info["lba"] - previous_data["lba"],
                        detail=f"prev_lba={previous_data['lba']}, curr_lba={info['lba']}",
                    ))

                dt = u32_delta(tick_ms, prev_tick)
                tick_deltas.append(dt)
                if dt >= stall_threshold_ms:
                    tick_stall_count += 1
                    events.append(make_event(
                        "TICK_STALL",
                        info["lba"],
                        seq=seq,
                        boot=boot,
                        value=dt,
                        detail=f"prev_tick={prev_tick}, curr_tick={tick_ms}",
                    ))

        previous_data = info

    integrity_fail_total = payload_crc_fail + block_crc_fail + linear_pattern_fail + dummy_magic_fail

    judgements = []
    if len(data_infos) == 0 and len(unknown_infos) > 0:
        judgements.append("유효 data block이 없고 unknown이 많다. 4bit 배선/DAT lane/버스 무결성 먼저 의심.")
    if integrity_fail_total > 0 and missing_seq_blocks == 0 and actual_lba_gap_count == 0:
        judgements.append("seq/LBA는 이어지는데 payload 검증이 실패한다. 상태 전환보다 버스/배선/클럭 무결성 쪽 가능성이 크다.")
    if latest_super is not None and unknown_before_next_data_lba == 0 and unknown_after_next_data_lba > 0:
        judgements.append("latest superblock 기준 next_data_lba 뒤쪽 unknown은 아직 안 쓴 tail로 보는 게 맞다.")
    if dummy_mode_count > 0 and integrity_fail_total == 0:
        judgements.append("현재 payload는 dummy packet 포맷으로 보이고, 기존 linear test parser가 오판했을 가능성이 크다.")
    if (tick_stall_count + embedded_stall_count) > 0 and integrity_fail_total == 0:
        judgements.append("연속성/CRC는 유지되고 stall만 보인다. ping-pong staging과 SD DMA 확장을 계속 진행해도 된다.")
    if boot_change_count > 0:
        judgements.append("boot_count 변화가 있다. reset/timeout/re-init 흔적과 에러 핸들러 경로를 확인하는 게 좋다.")
    if not judgements:
        judgements.append("뚜렷한 seq/LBA/CRC 이상이 없다.")

    lines = []
    lines.append("[Range]")
    lines.append(f"  sectors={len(infos)}")
    if infos:
        lines.append(f"  lba={infos[0]['lba']}..{infos[-1]['lba']}")
    lines.append("")

    lines.append("[Kinds]")
    for kind, count in sorted(counts.items()):
        lines.append(f"  {kind}: {count}")
    lines.append("")

    lines.append("[Payload]")
    lines.append(f"  dummy_packets={dummy_mode_count}")
    lines.append(f"  linear_test={linear_mode_count}")
    lines.append(f"  sensor_dma={sensor_dma_mode_count}")
    lines.append(f"  unknown_mode={unknown_payload_mode_count}")
    lines.append("")

    lines.append("[Continuity]")
    lines.append(f"  valid_data_blocks={len(data_infos)}")
    lines.append(f"  valid_superblocks={len(super_infos)}")
    lines.append(f"  unknown_blocks={len(unknown_infos)}")
    lines.append(f"  unknown_before_next_data_lba={unknown_before_next_data_lba}")
    lines.append(f"  unknown_after_next_data_lba={unknown_after_next_data_lba}")
    lines.append(f"  boot_changes={boot_change_count}")
    lines.append(f"  seq_gap_events={seq_gap_count}")
    lines.append(f"  missing_seq_blocks_est={missing_seq_blocks}")
    lines.append(f"  duplicate_seq={duplicate_seq_count}")
    lines.append(f"  backward_seq={backward_seq_count}")
    lines.append(f"  actual_lba_gaps={actual_lba_gap_count}")
    lines.append(f"  declared_lba_mismatch={declared_lba_mismatch}")
    lines.append("")

    lines.append("[Timing]")
    lines.append(f"  stall_threshold_ms={stall_threshold_ms}")
    lines.append(f"  tick_delta_ms: {summarize_ms(tick_deltas)}")
    lines.append(f"  cycle_data_ms: {summarize_ms(cycle_data_ms_values)}")
    lines.append(f"  cycle_superblock_ms: {summarize_ms(cycle_super_ms_values)}")
    lines.append(f"  cycle_total_ms: {summarize_ms(cycle_total_ms_values)}")
    lines.append(f"  tick_stalls={tick_stall_count}")
    lines.append(f"  cycle_stalls={embedded_stall_count}")
    lines.append("")

    lines.append("[Integrity]")
    lines.append(f"  payload_crc_fail={payload_crc_fail}")
    lines.append(f"  block_crc_fail={block_crc_fail}")
    lines.append(f"  dummy_packet_magic_fail={dummy_magic_fail}")
    lines.append(f"  linear_pattern_fail={linear_pattern_fail}")
    lines.append(f"  legacy_linear_pattern_forced={force_linear_pattern_check}")
    lines.append("")

    if latest_super is not None:
        lines.append("[Latest superblock]")
        lines.append(f"  kind={latest_super.get('kind', '')}")
        lines.append(f"  lba={latest_super.get('lba', '')}")
        lines.append(f"  write_seq={latest_super.get('write_seq', '')}")
        lines.append(f"  boot_count={latest_super.get('boot_count', '')}")
        lines.append(f"  next_data_lba={latest_super.get('next_data_lba', '')}")
        lines.append(f"  last_total_write_ms={latest_super.get('last_total_write_ms', '')}")
        lines.append(f"  max_total_write_ms={latest_super.get('max_total_write_ms', '')}")
        lines.append(f"  stall_count={latest_super.get('stall_count', '')}")
        lines.append("")

    lines.append("[Judgement]")
    for item in judgements:
        lines.append(f"  - {item}")

    return "\n".join(lines), events


class App:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("1440x960")
        self.drives = scan_physical_drives()

        self.target_var = tk.StringVar(value=self.drives[0]["target"] if self.drives else r"\\.\PhysicalDrive0")
        self.lba_var = tk.StringVar(value="0")
        self.sector_size_var = tk.StringVar(value=str(DEFAULT_SECTOR_SIZE))
        self.export_start_var = tk.StringVar(value="0")
        self.export_end_var = tk.StringVar(value="63")
        self.export_mode_var = tk.StringVar(value="parsed")
        self.erase_start_var = tk.StringVar(value="0")
        self.erase_end_var = tk.StringVar(value="31")
        self.analysis_start_var = tk.StringVar(value="0")
        self.analysis_end_var = tk.StringVar(value="63")
        self.analysis_stall_threshold_var = tk.StringVar(value="20")
        self.analysis_assume_pattern_var = tk.BooleanVar(value=False)
        self.status_var = tk.StringVar(value="대기 중")
        self.drive_info_var = tk.StringVar(value="")
        self.progress_var = tk.DoubleVar(value=0.0)
        self.progress_text_var = tk.StringVar(value="")

        self.read_button = None
        self.export_button = None
        self.erase_button = None
        self.erase_range_button = None
        self.analyze_button = None
        self.drive_combo = None
        self.table = None
        self.hex_text = None
        self.parsed_text = None
        self.analysis_summary_text = None
        self.analysis_events_tree = None
        self.analysis_events: list[dict] = []
        self.bottom_pane = None

        self._build_ui()
        self._install_nav_keys()
        self.target_var.trace_add("write", self._on_target_var_changed)
        self.refresh_drive_info()
        self.root.after(100, self._set_initial_pane_ratio)

    def _build_ui(self):
        top = ttk.Frame(self.root, padding=10)
        top.pack(fill="x")

        ttk.Label(top, text="Physical drive").grid(row=0, column=0, sticky="w")
        values = [d["target"] for d in self.drives] or [r"\\.\PhysicalDrive0"]
        combo = ttk.Combobox(top, textvariable=self.target_var, values=values, width=28, state="readonly")
        combo.grid(row=0, column=1, sticky="w", padx=(6, 12))
        combo.bind("<<ComboboxSelected>>", self._on_drive_combo_selected)
        self.drive_combo = combo

        ttk.Label(top, text="Sector size").grid(row=0, column=2, sticky="w")
        ttk.Entry(top, textvariable=self.sector_size_var, width=8).grid(row=0, column=3, sticky="w", padx=(6, 12))

        ttk.Label(top, text="Center LBA").grid(row=0, column=4, sticky="w")
        ttk.Entry(top, textvariable=self.lba_var, width=12).grid(row=0, column=5, sticky="w", padx=(6, 12))

        self.read_button = ttk.Button(top, text="Read [LBA-1, LBA, LBA+1]", command=self.read)
        self.read_button.grid(row=0, column=6, sticky="e")

        info = (
            "RAW-only mode: drive letters는 무시하고 PhysicalDrive만 본다.\n"
            "카드를 STM32가 raw로 덮은 뒤에는 Windows에서 포맷하라는 창이 떠도 정상이다.\n"
            f"단축키: A=이전 페이지, D=다음 페이지 (페이지 간격 {LBA_PAGE_STEP} LBA)"
        )
        ttk.Label(self.root, text=info, padding=(10, 0, 10, 6), foreground="#444444").pack(anchor="w")
        ttk.Label(self.root, textvariable=self.drive_info_var, padding=(10, 0, 10, 8), foreground="#1f4f7f").pack(anchor="w")

        table = ttk.Treeview(self.root, columns=("target", "name", "bus", "size", "flags"), show="headings", height=6)
        table.pack(fill="x", padx=10, pady=(0, 8))
        self.table = table
        for column, width in (
            ("target", 170),
            ("name", 390),
            ("bus", 80),
            ("size", 180),
            ("flags", 170),
        ):
            table.heading(column, text=column)
            table.column(column, width=width, anchor="w")
        for idx, drive in enumerate(self.drives):
            table.insert(
                "",
                "end",
                iid=str(idx),
                values=(
                    drive["target"],
                    drive["name"],
                    drive["bus"],
                    f"{size_text(drive['size'])} / {decimal_gb_text(drive['size'])}",
                    device_flags_text(drive),
                ),
            )
        table.bind("<Double-1>", self.use_selected)

        tools = ttk.LabelFrame(self.root, text="도구", padding=10)
        tools.pack(fill="x", padx=10, pady=(0, 8))

        ttk.Label(tools, text="CSV start LBA").grid(row=0, column=0, sticky="w")
        ttk.Entry(tools, textvariable=self.export_start_var, width=12).grid(row=0, column=1, sticky="w", padx=(6, 12))

        ttk.Label(tools, text="CSV end LBA").grid(row=0, column=2, sticky="w")
        ttk.Entry(tools, textvariable=self.export_end_var, width=12).grid(row=0, column=3, sticky="w", padx=(6, 12))

        ttk.Label(tools, text="CSV mode").grid(row=0, column=4, sticky="w")
        ttk.Combobox(
            tools,
            textvariable=self.export_mode_var,
            values=("parsed", "hex"),
            width=10,
            state="readonly",
        ).grid(row=0, column=5, sticky="w", padx=(6, 12))

        self.export_button = ttk.Button(tools, text="Export CSV", command=self.export_csv)
        self.export_button.grid(row=0, column=6, sticky="w", padx=(0, 12))

        self.erase_button = ttk.Button(tools, text="Zero wipe selected drive", command=self.erase_selected_drive)
        self.erase_button.grid(row=0, column=7, sticky="w")

        ttk.Label(tools, text="Erase start LBA").grid(row=1, column=0, sticky="w", pady=(8, 0))
        ttk.Entry(tools, textvariable=self.erase_start_var, width=12).grid(row=1, column=1, sticky="w", padx=(6, 12), pady=(8, 0))

        ttk.Label(tools, text="Erase end LBA").grid(row=1, column=2, sticky="w", pady=(8, 0))
        ttk.Entry(tools, textvariable=self.erase_end_var, width=12).grid(row=1, column=3, sticky="w", padx=(6, 12), pady=(8, 0))

        self.erase_range_button = ttk.Button(tools, text="Zero wipe selected range", command=self.erase_selected_range)
        self.erase_range_button.grid(row=1, column=4, columnspan=2, sticky="w", padx=(0, 12), pady=(8, 0))

        ttk.Label(
            tools,
            text="범위 wipe와 전체 wipe 모두 40GB 이하 드라이브만 허용. 추가로 BOOT/SYSTEM/OFFLINE/READONLY도 차단.",
            foreground="#7a3b00",
        ).grid(row=2, column=0, columnspan=8, sticky="w", pady=(8, 0))

        progress_frame = ttk.Frame(self.root, padding=(10, 0, 10, 8))
        progress_frame.pack(fill="x")
        ttk.Progressbar(progress_frame, variable=self.progress_var, maximum=100.0).pack(fill="x", side="top")
        ttk.Label(progress_frame, textvariable=self.progress_text_var).pack(anchor="w", pady=(4, 0))
        ttk.Label(self.root, textvariable=self.status_var, padding=(10, 0, 10, 8)).pack(anchor="w")

        bottom_pane = ttk.Panedwindow(self.root, orient="horizontal")
        bottom_pane.pack(fill="both", expand=True, padx=10, pady=(0, 10))
        self.bottom_pane = bottom_pane

        left_frame = ttk.Frame(bottom_pane)
        right_frame = ttk.Frame(bottom_pane)
        bottom_pane.add(left_frame, weight=3)
        bottom_pane.add(right_frame, weight=2)

        left_notebook = ttk.Notebook(left_frame)
        left_notebook.pack(fill="both", expand=True)

        self.hex_text = ScrolledText(left_notebook, wrap="none", font=("Consolas", 10))
        self.parsed_text = ScrolledText(left_notebook, wrap="word", font=("Consolas", 10))
        left_notebook.add(self.hex_text, text="HEX")
        left_notebook.add(self.parsed_text, text="PARSED")
        for widget in (self.hex_text, self.parsed_text):
            widget.configure(state="disabled")

        analysis_controls = ttk.LabelFrame(right_frame, text="분석", padding=10)
        analysis_controls.pack(fill="x", pady=(0, 8))

        ttk.Label(analysis_controls, text="Start LBA").grid(row=0, column=0, sticky="w")
        ttk.Entry(analysis_controls, textvariable=self.analysis_start_var, width=12).grid(row=0, column=1, sticky="w", padx=(6, 12))

        ttk.Label(analysis_controls, text="End LBA").grid(row=0, column=2, sticky="w")
        ttk.Entry(analysis_controls, textvariable=self.analysis_end_var, width=12).grid(row=0, column=3, sticky="w", padx=(6, 12))

        ttk.Label(analysis_controls, text="Stall ms").grid(row=1, column=0, sticky="w", pady=(8, 0))
        ttk.Entry(analysis_controls, textvariable=self.analysis_stall_threshold_var, width=12).grid(row=1, column=1, sticky="w", padx=(6, 12), pady=(8, 0))

        ttk.Checkbutton(
            analysis_controls,
            text="Force legacy linear pattern check",
            variable=self.analysis_assume_pattern_var,
        ).grid(row=1, column=2, columnspan=2, sticky="w", pady=(8, 0))

        self.analyze_button = ttk.Button(analysis_controls, text="Analyze range", command=self.analyze_range)
        self.analyze_button.grid(row=0, column=4, rowspan=2, sticky="nsw", padx=(6, 8))

        ttk.Button(analysis_controls, text="Clear", command=self.clear_analysis).grid(
            row=0, column=5, rowspan=2, sticky="nsw"
        )

        ttk.Label(
            analysis_controls,
            text="stall은 tick delta와 on-block cycle time 둘 다 본다. legacy linear pattern 검사는 old test payload일 때만 강제로 켜면 된다.",
            foreground="#555555",
        ).grid(row=2, column=0, columnspan=6, sticky="w", pady=(8, 0))

        analysis_notebook = ttk.Notebook(right_frame)
        analysis_notebook.pack(fill="both", expand=True)

        summary_frame = ttk.Frame(analysis_notebook)
        events_frame = ttk.Frame(analysis_notebook)
        analysis_notebook.add(summary_frame, text="SUMMARY")
        analysis_notebook.add(events_frame, text="EVENTS")

        self.analysis_summary_text = ScrolledText(summary_frame, wrap="word", font=("Consolas", 10))
        self.analysis_summary_text.pack(fill="both", expand=True)
        self.analysis_summary_text.configure(state="disabled")

        columns = ("type", "lba", "seq", "boot", "value", "detail")
        events_tree = ttk.Treeview(events_frame, columns=columns, show="headings")
        self.analysis_events_tree = events_tree
        for column, width in (
            ("type", 130),
            ("lba", 80),
            ("seq", 90),
            ("boot", 70),
            ("value", 90),
            ("detail", 520),
        ):
            events_tree.heading(column, text=column)
            events_tree.column(column, width=width, anchor="w")
        events_tree.pack(fill="both", expand=True, side="left")
        events_tree.bind("<Double-1>", self.on_analysis_event_open)

        scrollbar = ttk.Scrollbar(events_frame, orient="vertical", command=events_tree.yview)
        scrollbar.pack(fill="y", side="right")
        events_tree.configure(yscrollcommand=scrollbar.set)

    def _set_initial_pane_ratio(self):
        if self.bottom_pane is None:
            return
        try:
            total = max(self.bottom_pane.winfo_width(), 100)
            self.bottom_pane.sashpos(0, int(total * 0.60))
        except Exception:
            pass

    def _install_nav_keys(self):
        self.root.bind_class(NAV_BINDTAG, "<KeyPress>", self.on_global_key)
        self._prepend_bindtag_recursive(self.root)

    def _prepend_bindtag_recursive(self, widget):
        tags = list(widget.bindtags())
        if NAV_BINDTAG in tags:
            tags.remove(NAV_BINDTAG)
        widget.bindtags((NAV_BINDTAG, *tags))
        for child in widget.winfo_children():
            self._prepend_bindtag_recursive(child)

    def _on_target_var_changed(self, *_args):
        self.refresh_drive_info()

    def _on_drive_combo_selected(self, _event=None):
        self.refresh_drive_info()

    def on_global_key(self, event):
        key = event.keysym.lower()
        if key == "a":
            self.change_page(-LBA_PAGE_STEP)
            return "break"
        if key == "d":
            self.change_page(LBA_PAGE_STEP)
            return "break"
        return None

    def use_selected(self, event=None):
        selection = self.table.selection()
        if not selection:
            return
        idx = int(selection[0])
        self.target_var.set(self.drives[idx]["target"])
        self.refresh_drive_info()

    def set_text(self, widget: ScrolledText, text: str):
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        widget.insert("1.0", text)
        widget.configure(state="disabled")

    def set_status(self, text: str):
        self.status_var.set(text)
        self.root.update_idletasks()

    def set_progress(self, value: float, text: str):
        self.progress_var.set(max(0.0, min(100.0, value)))
        self.progress_text_var.set(text)
        self.root.update_idletasks()

    def clear_progress(self):
        self.progress_var.set(0.0)
        self.progress_text_var.set("")
        self.root.update_idletasks()

    def set_busy(self, busy: bool):
        state = "disabled" if busy else "normal"
        if self.read_button is not None:
            self.read_button.configure(state=state)
        if self.export_button is not None:
            self.export_button.configure(state=state)
        if self.erase_range_button is not None:
            self.erase_range_button.configure(state=state)
        if self.analyze_button is not None:
            self.analyze_button.configure(state=state)
        if self.drive_combo is not None:
            self.drive_combo.configure(state="disabled" if busy else "readonly")
        self.refresh_drive_info(force_busy=busy)
        self.root.update_idletasks()

    def get_selected_drive(self) -> dict | None:
        target = self.target_var.get()
        for drive in self.drives:
            if drive["target"].lower() == target.lower():
                return drive
        return None

    def get_sector_size(self) -> int:
        try:
            sector_size = int(self.sector_size_var.get(), 0)
        except ValueError as exc:
            raise RawDeviceError("sector_size가 숫자가 아니다.") from exc
        if sector_size <= 0:
            raise RawDeviceError("sector_size는 0보다 커야 한다.")
        return sector_size

    def get_total_lbas_for_selected_drive(self) -> int:
        drive = self.get_selected_drive()
        if drive is None:
            raise RawDeviceError("선택한 drive 정보를 찾지 못했다.")
        sector_size = self.get_sector_size()
        total_lbas = drive["size"] // sector_size
        if total_lbas <= 0:
            raise RawDeviceError("계산된 total_lbas가 0 이하이다.")
        return total_lbas

    def is_erase_allowed(self, drive: dict | None) -> tuple[bool, str]:
        if drive is None:
            return False, "선택된 드라이브 정보를 찾지 못했다."
        if drive["size"] > MAX_ERASE_BYTES:
            return False, f"드라이브 크기가 {decimal_gb_text(drive['size'])}라서 40GB 제한을 넘는다."
        if drive.get("is_boot"):
            return False, "BOOT 드라이브는 차단했다."
        if drive.get("is_system"):
            return False, "SYSTEM 드라이브는 차단했다."
        if drive.get("is_offline"):
            return False, "OFFLINE 드라이브는 차단했다."
        if drive.get("is_readonly"):
            return False, "READONLY 드라이브는 차단했다."
        return True, "삭제 허용"

    def refresh_drive_info(self, force_busy: bool = False):
        drive = self.get_selected_drive()
        if drive is None:
            self.drive_info_var.set("선택한 드라이브 정보를 찾지 못했다.")
            if self.erase_button is not None:
                self.erase_button.configure(state="disabled")
            if self.erase_range_button is not None:
                self.erase_range_button.configure(state="disabled")
            return

        allowed, reason = self.is_erase_allowed(drive)
        info = (
            f"선택 드라이브: {drive['target']} / {drive['name']} / {drive['bus']} / "
            f"{size_text(drive['size'])} ({decimal_gb_text(drive['size'])}) / "
            f"logical_sector={drive['logical_sector_size']} / flags={device_flags_text(drive)} / "
            f"wipe={'허용' if allowed else '차단'}: {reason}"
        )
        self.drive_info_var.set(info)

        target_state = "disabled" if force_busy else ("normal" if allowed else "disabled")
        if self.erase_button is not None:
            self.erase_button.configure(state=target_state)
        if self.erase_range_button is not None:
            self.erase_range_button.configure(state=target_state)

    def change_page(self, delta: int):
        try:
            center = int(self.lba_var.get(), 0)
        except ValueError:
            center = 0
        new_center = center + delta
        if new_center < 0:
            new_center = 0
        self.lba_var.set(str(new_center))
        self.read()

    def read(self):
        try:
            target = self.target_var.get()
            sector_size = self.get_sector_size()
            center = int(self.lba_var.get(), 0)
            total_lbas = self.get_total_lbas_for_selected_drive()
            if center >= total_lbas:
                raise RawDeviceError(f"center LBA가 범위를 넘었다. max={total_lbas - 1}")

            lbas = [x for x in (center - 1, center, center + 1) if 0 <= x < total_lbas]
            sectors = []
            self.set_status(f"읽는 중: {target} / lbas={lbas}")
            with RawDeviceReader(target) as reader:
                for lba in lbas:
                    data = reader.read_lba(lba, sector_size, 1)
                    if len(data) != sector_size:
                        raise RawDeviceError(f"LBA {lba} short read")
                    sectors.append((lba, data))

            hex_parts = []
            parsed_parts = [f"target={target}", f"center_lba={center}", ""]
            for lba, data in sectors:
                info = parse_sector_info(lba, data)
                hex_parts.append(f"================ LBA {lba}{' <== center' if lba == center else ''} ================")
                hex_parts.append(hexdump(data))
                hex_parts.append("")
                parsed_parts.append(f"================ LBA {lba}{' <== center' if lba == center else ''} ================")
                parsed_parts.append(format_parsed_info(info))
                parsed_parts.append("")

            self.set_text(self.hex_text, "\n".join(hex_parts))
            self.set_text(self.parsed_text, "\n".join(parsed_parts))
            self.set_status(f"읽기 성공: {target} / lbas={lbas}")
        except Exception as exc:
            self.set_status(f"읽기 실패: {exc}")
            messagebox.showerror(APP_TITLE, str(exc))

    def _row_for_export(self, target: str, sector_size: int, info: dict, data: bytes) -> dict:
        row = {field: "" for field in PARSED_EXPORT_FIELDS}
        row["target"] = target
        row["sector_size"] = sector_size
        for field in PARSED_EXPORT_FIELDS:
            if field in info:
                row[field] = info[field]
        row["parsed_text"] = format_parsed_info(info)
        return row

    def export_csv(self):
        try:
            target = self.target_var.get()
            sector_size = self.get_sector_size()
            total_lbas = self.get_total_lbas_for_selected_drive()
            start_lba = int(self.export_start_var.get(), 0)
            end_lba = int(self.export_end_var.get(), 0)
            mode = self.export_mode_var.get().strip().lower()

            if start_lba < 0 or end_lba < start_lba:
                raise RawDeviceError("CSV LBA 범위가 잘못되었다.")
            if end_lba >= total_lbas:
                raise RawDeviceError(f"CSV end LBA가 범위를 넘었다. max={total_lbas - 1}")
            if mode not in ("hex", "parsed"):
                raise RawDeviceError("CSV mode는 hex 또는 parsed여야 한다.")

            default_name = f"raw_export_{mode}_{start_lba}_{end_lba}.csv"
            save_path = filedialog.asksaveasfilename(
                title="CSV 저장",
                defaultextension=".csv",
                initialfile=default_name,
                filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
            )
            if not save_path:
                return

            count = end_lba - start_lba + 1
            self.set_busy(True)
            self.set_progress(0.0, f"CSV 준비 중: {count}개 LBA")
            self.set_status(f"CSV 추출 중: {target} / {start_lba}..{end_lba} / mode={mode}")

            with RawDeviceReader(target) as reader, open(save_path, "w", newline="", encoding="utf-8-sig") as fp:
                if mode == "hex":
                    writer = csv.DictWriter(fp, fieldnames=HEX_EXPORT_FIELDS)
                    writer.writeheader()
                else:
                    writer = csv.DictWriter(fp, fieldnames=PARSED_EXPORT_FIELDS)
                    writer.writeheader()

                for idx, lba in enumerate(range(start_lba, end_lba + 1), start=1):
                    data = reader.read_lba(lba, sector_size, 1)
                    if len(data) != sector_size:
                        raise RawDeviceError(f"LBA {lba} short read")
                    info = parse_sector_info(lba, data)

                    if mode == "hex":
                        row = {
                            "target": target,
                            "sector_size": sector_size,
                            "lba": lba,
                            "size": info["size"],
                            "unique_bytes": info["unique_bytes"],
                            "zeros": info["zeros"],
                            "first_16": info["first_16"],
                            "last_16": info["last_16"],
                            "ascii_preview": info["ascii_preview"],
                            "raw_hex": info["raw_hex"],
                            "hex_dump": hexdump(data),
                        }
                    else:
                        row = self._row_for_export(target, sector_size, info, data)

                    writer.writerow(row)

                    if idx == 1 or idx == count or (idx % 16) == 0:
                        pct = 100.0 * idx / count
                        self.set_progress(pct, f"CSV 추출 {idx}/{count} LBA 완료")
                        self.set_status(f"CSV 추출 중: {target} / {start_lba}..{end_lba} / mode={mode} / {idx}/{count}")

            self.set_status(f"CSV 저장 완료: {save_path}")
            self.set_progress(100.0, f"CSV 완료: {os.path.basename(save_path)}")
            messagebox.showinfo(APP_TITLE, f"CSV 저장 완료\n{save_path}")
        except Exception as exc:
            self.set_status(f"CSV 추출 실패: {exc}")
            messagebox.showerror(APP_TITLE, str(exc))
        finally:
            self.set_busy(False)

    def clear_analysis(self):
        self.analysis_events = []
        if self.analysis_events_tree is not None:
            for item in self.analysis_events_tree.get_children():
                self.analysis_events_tree.delete(item)
        if self.analysis_summary_text is not None:
            self.set_text(self.analysis_summary_text, "")

    def _populate_analysis_events(self, events: list[dict]):
        self.analysis_events = events
        tree = self.analysis_events_tree
        if tree is None:
            return
        for item in tree.get_children():
            tree.delete(item)
        for idx, event in enumerate(events):
            tree.insert(
                "",
                "end",
                iid=str(idx),
                values=(
                    event.get("type", ""),
                    event.get("lba", ""),
                    event.get("seq", ""),
                    event.get("boot", ""),
                    event.get("value", ""),
                    event.get("detail", ""),
                ),
            )

    def on_analysis_event_open(self, event=None):
        if self.analysis_events_tree is None:
            return
        selection = self.analysis_events_tree.selection()
        if not selection:
            return
        idx = int(selection[0])
        if idx < 0 or idx >= len(self.analysis_events):
            return
        lba = self.analysis_events[idx].get("lba")
        if lba in (None, ""):
            return
        self.lba_var.set(str(lba))
        self.read()

    def analyze_range(self):
        try:
            target = self.target_var.get()
            sector_size = self.get_sector_size()
            total_lbas = self.get_total_lbas_for_selected_drive()
            start_lba = int(self.analysis_start_var.get(), 0)
            end_lba = int(self.analysis_end_var.get(), 0)
            stall_threshold_ms = int(self.analysis_stall_threshold_var.get(), 0)
            assume_dummy_pattern = bool(self.analysis_assume_pattern_var.get())

            if start_lba < 0 or end_lba < start_lba:
                raise RawDeviceError("분석 LBA 범위가 잘못되었다.")
            if end_lba >= total_lbas:
                raise RawDeviceError(f"분석 end LBA가 범위를 넘었다. max={total_lbas - 1}")
            if stall_threshold_ms <= 0:
                raise RawDeviceError("stall threshold는 0보다 커야 한다.")

            count = end_lba - start_lba + 1
            infos: list[dict] = []

            self.set_busy(True)
            self.set_progress(0.0, f"분석 준비 중: {count}개 LBA")
            self.set_status(f"분석 중: {target} / {start_lba}..{end_lba}")

            with RawDeviceReader(target) as reader:
                chunk_lbas = 64
                done = 0
                current = start_lba
                while current <= end_lba:
                    this_lbas = min(chunk_lbas, end_lba - current + 1)
                    block = reader.read_lba(current, sector_size, this_lbas)
                    if len(block) != sector_size * this_lbas:
                        raise RawDeviceError(f"LBA {current}..{current + this_lbas - 1} short read")

                    for idx in range(this_lbas):
                        off = idx * sector_size
                        data = block[off:off + sector_size]
                        infos.append(parse_sector_info(current + idx, data))

                    done += this_lbas
                    current += this_lbas
                    pct = 100.0 * done / count
                    self.set_progress(pct, f"분석 {done}/{count} LBA 완료")
                    self.set_status(f"분석 중: {target} / {start_lba}..{end_lba} / {done}/{count}")

            summary_text, events = analyze_infos(infos, stall_threshold_ms, assume_dummy_pattern)
            self.set_text(self.analysis_summary_text, summary_text)
            self._populate_analysis_events(events)
            self.set_progress(100.0, f"분석 완료: anomalies={len(events)}")
            self.set_status(f"분석 완료: {target} / {start_lba}..{end_lba} / anomalies={len(events)}")
        except Exception as exc:
            self.set_status(f"분석 실패: {exc}")
            messagebox.showerror(APP_TITLE, str(exc))
        finally:
            self.set_busy(False)

    def erase_selected_range(self):
        try:
            drive = self.get_selected_drive()
            allowed, reason = self.is_erase_allowed(drive)
            if not allowed:
                raise RawDeviceError(reason)

            target = drive["target"]
            sector_size = self.get_sector_size()
            total_lbas = drive["size"] // sector_size
            if total_lbas <= 0:
                raise RawDeviceError("계산된 total_lbas가 0 이하이다.")

            start_lba = int(self.erase_start_var.get(), 0)
            end_lba = int(self.erase_end_var.get(), 0)
            if start_lba < 0 or end_lba < start_lba:
                raise RawDeviceError("Erase LBA 범위가 잘못되었다.")
            if end_lba >= total_lbas:
                raise RawDeviceError(f"Erase end LBA가 범위를 넘었다. max={total_lbas - 1}")

            erase_count = end_lba - start_lba + 1
            erase_bytes = erase_count * sector_size
            confirm_text = "\n".join([
                "선택한 PhysicalDrive의 지정 범위를 0x00으로 덮어쓴다.",
                f"target={target}",
                f"size={size_text(drive['size'])} ({decimal_gb_text(drive['size'])})",
                f"sector_size={sector_size}",
                f"lba_range={start_lba}..{end_lba}",
                f"lba_count={erase_count}",
                f"bytes={erase_bytes} ({size_text(erase_bytes)})",
                "",
                "계속할까?",
            ])

            ok = messagebox.askyesno(APP_TITLE, confirm_text, icon="warning")
            if not ok:
                return

            token = simpledialog.askstring(APP_TITLE, "계속하려면 EXACTLY 'ERASE' 입력")
            if token != "ERASE":
                self.set_status("범위 삭제 취소됨")
                return

            self.set_busy(True)
            self.set_progress(0.0, "Range zero wipe 시작")
            self.set_status(f"Range zero wipe 중: {target} / {start_lba}..{end_lba}")

            def progress_cb(done, total, current_lba, mib_per_sec):
                pct = 100.0 * done / total
                self.set_progress(
                    pct,
                    f"Range zero wipe {done}/{total} LBA / current={current_lba} / speed={mib_per_sec:.2f} MiB/s",
                )
                self.set_status(
                    f"Range zero wipe 중: {target} / {start_lba}..{end_lba} / {done}/{total} LBA 완료"
                )

            with RawDeviceWriter(target) as writer:
                writer.zero_fill_lba_range(
                    start_lba=start_lba,
                    end_lba=end_lba,
                    sector_size=sector_size,
                    chunk_lbas=128,
                    progress_cb=progress_cb,
                )

            self.set_progress(100.0, "Range zero wipe 완료")
            self.set_status(f"Range zero wipe 완료: {target} / {start_lba}..{end_lba}")
            messagebox.showinfo(APP_TITLE, f"선택 범위 zero wipe 완료\n{start_lba}..{end_lba}")
        except Exception as exc:
            self.set_status(f"Range zero wipe 실패: {exc}")
            messagebox.showerror(APP_TITLE, str(exc))
        finally:
            self.set_busy(False)

    def erase_selected_drive(self):
        try:
            drive = self.get_selected_drive()
            allowed, reason = self.is_erase_allowed(drive)
            if not allowed:
                raise RawDeviceError(reason)

            target = drive["target"]
            sector_size = self.get_sector_size()
            total_lbas = drive["size"] // sector_size
            if total_lbas <= 0:
                raise RawDeviceError("계산된 total_lbas가 0 이하이다.")

            ok = messagebox.askyesno(
                APP_TITLE,
                "선택한 PhysicalDrive 전체를 0x00으로 덮어쓴다.\n"
                f"target={target}\n"
                f"size={size_text(drive['size'])} ({decimal_gb_text(drive['size'])})\n"
                f"sector_size={sector_size}\n"
                f"lba_range=0..{total_lbas - 1}\n\n"
                "계속할까?",
                icon="warning",
            )
            if not ok:
                return

            token = simpledialog.askstring(APP_TITLE, "계속하려면 EXACTLY 'ERASE' 입력")
            if token != "ERASE":
                self.set_status("삭제 취소됨")
                return

            self.set_busy(True)
            self.set_progress(0.0, "Zero wipe 시작")
            self.set_status(f"Zero wipe 중: {target} / 0..{total_lbas - 1}")

            def progress_cb(done, total, current_lba, mib_per_sec):
                pct = 100.0 * done / total
                self.set_progress(
                    pct,
                    f"Zero wipe {done}/{total} LBA / current={current_lba} / speed={mib_per_sec:.2f} MiB/s",
                )
                self.set_status(f"Zero wipe 중: {target} / {done}/{total} LBA 완료")

            with RawDeviceWriter(target) as writer:
                writer.zero_fill_lba_range(
                    start_lba=0,
                    end_lba=total_lbas - 1,
                    sector_size=sector_size,
                    chunk_lbas=128,
                    progress_cb=progress_cb,
                )

            self.set_progress(100.0, "Zero wipe 완료")
            self.set_status(f"Zero wipe 완료: {target}")
            messagebox.showinfo(APP_TITLE, "선택 드라이브 zero wipe 완료")
        except Exception as exc:
            self.set_status(f"Zero wipe 실패: {exc}")
            messagebox.showerror(APP_TITLE, str(exc))
        finally:
            self.set_busy(False)

    def read_initial_view(self):
        try:
            self.read()
        except Exception:
            pass



def main():
    if os.name != "nt":
        print("Windows 전용")
        return 1
    root = tk.Tk()
    app = App(root)
    root.after(100, app.read_initial_view)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
