"""Convert the diagnostic RGB PPM to PNG using only Python's standard library."""
import argparse
import struct
import zlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("input", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
magic, dimensions, max_value, data = args.input.read_bytes().split(b"\n", 3)
width, height = map(int, dimensions.split())
if magic != b"P6" or max_value != b"255" or len(data) != width * height * 3:
    raise ValueError("Expected an RGB8 PPM from pvz_capture_frame")

def chunk(kind, payload):
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF))

filtered = b"".join(b"\0" + data[y * width * 3:(y + 1) * width * 3] for y in range(height))
args.output.write_bytes(b"\x89PNG\r\n\x1a\n" +
    chunk(b"IHDR", struct.pack(">2I5B", width, height, 8, 2, 0, 0, 0)) +
    chunk(b"IDAT", zlib.compress(filtered)) + chunk(b"IEND", b""))
