#!/usr/bin/env python3
"""Receive a PicoVoxel UART RGB565 framebuffer snapshot and save it as a BMP file."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path
from typing import BinaryIO

MAGIC = b"SNAP_BEGIN\n"
DATA_MARKER = b"DATA\n"
END_MARKER = b"\nSNAP_END\n"


def read_exact(stream: BinaryIO, length: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < length:
        chunk = stream.read(length - len(chunks))
        if not chunk:
            raise EOFError(f"expected {length} bytes, got {len(chunks)} bytes")
        chunks.extend(chunk)
    return bytes(chunks)


def read_until_magic(stream: BinaryIO) -> None:
    window = bytearray()
    while True:
        byte = stream.read(1)
        if not byte:
            raise EOFError("snapshot magic not found")
        window.extend(byte)
        if len(window) > len(MAGIC):
            del window[0: len(window) - len (MAGIC)]
        if bytes(window) == MAGIC:
            return


def read_header(stream: BinaryIO) -> dict[str, str]:
    header = bytearray()
    while not header.endswith(DATA_MARKER):
        byte = stream.read(1)
        if not byte:
            raise EOFError("snapshot header was not completed")
        header.extend(byte)
        if len(header) > 1024:
            raise ValueError("snapshot header is too large")

    fields: dict[str, str] = {}
    for raw_line in header[: -len(DATA_MARKER)].decode("ascii").splitlines():
        key, _, value = raw_line.partition(" ")
        fields[key] = value
    return fields


def rgb565le_to_bmp(pixel_data: bytes, width: int, height: int, output_path: Path) -> None:
    expected = width * height * 2
    if len(pixel_data) != expected:
        raise ValueError(f"expected {expected} RGB565 bytes, got {len(pixel_data)}")

    row_stride = ((width * 3 + 3) // 4) * 4
    pixel_array_size = row_stride * height
    file_size = 14 + 40 + pixel_array_size

    with output_path.open("wb") as bmp:
        bmp.write(b"BM")
        bmp.write(struct.pack("<IHHI", file_size, 0, 0, 14 + 40))
        bmp.write(struct.pack("<IIIHHIIIIII", 40, width, height, 1, 24, 0, pixel_array_size, 2835, 2835, 0, 0))

        padding = b"\x00" * (row_stride - width * 3)
        for y in range(height - 1, -1, -1):
            row = bytearray()
            row_offset = y * width * 2
            for x in range(width):
                value = pixel_data[row_offset + x * 2] | (pixel_data[row_offset + x * 2 + 1] << 8)
                red = ((value >> 11) & 0x1F) * 255 // 31
                green = ((value >> 5) & 0x3F) * 255 // 63
                blue = (value & 0x1F) * 255 // 31
                row.extend((blue, green, red))
            bmp.write(row)
            bmp.write(padding)


def receive_snapshot(stream: BinaryIO) -> tuple[int, int, bytes]:
    read_until_magic(stream)
    fields = read_header(stream)

    width = int(fields.get("WIDTH", "0"))
    height = int(fields.get("HEIGHT", "0"))
    byte_count = int(fields.get("BYTES", "0"))
    if width <= 0 or height <= 0 or byte_count != width * height * 2:
        raise ValueError(f"invalid snapshot header: {fields}")
    if fields.get("FORMAT") != "RGB565LE":
        raise ValueError(f"unsupported snapshot format: {fields.get('FORMAT')}")

    pixels = read_exact(stream, byte_count)
    end_marker = read_exact(stream, len(END_MARKER))
    if end_marker != END_MARKER:
        raise ValueError("snapshot end marker was not found after pixel data")

    return width, height, pixels


def open_serial(port: str, baudrate: int, timeout: float) -> BinaryIO:
    try:
        import serial
    except ImportError as exc:
        raise SystemExit("pyserial is required for serial capture: python -m pip install pyserial") from exc

    return serial.Serial(port=port, baudrate=baudrate, timeout=timeout)


def main() -> int:
    parser = argparse.ArgumentParser(description="Receive PicoVoxel UART framebuffer snapshot data and write a BMP file.")
    parser.add_argument("output", type=Path, help="output BMP path")
    parser.add_argument("--port", help="UART serial port, for example COMS or /dev/ttyUSB0")
    parser.add_argument("--baudrate", type=int, default=115200, help="UART baudrate")
    parser.add_argument("--timeout", type=float, default=10.0, help="serial read timeout in seconds")
    parser.add_argument("--input", type=Path, help="read a previously captured protocol stream instead of a serial port")
    args = parser.parse_args()

    if bool(args.port) == bool(args.input):
        parser.error("specify exactly one of --port or --input")

    try:
        if args.input:
            with args.input.open("rb") as stream:
                width, height, pixels = receive_snapshot(stream)
        else:
            with open_serial(args.port, args.baudrate, args.timeout) as stream:
                print("waiting for L3 snapshot on UART...", file=sys.stderr)
                width, height, pixels = receive_snapshot(stream)

        rgb565le_to_bmp(pixels, width, height, args.output)
    except (EOFError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print(f"wrote {args.output} ({width}x{height})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
