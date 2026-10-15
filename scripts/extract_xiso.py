"""List or extract files from an Xbox 360 disc image (XDVDFS, XGD2/XGD3).

    python scripts/extract_xiso.py <image.iso>                 # list
    python scripts/extract_xiso.py <image.iso> <out> [names]   # extract (all or named files)

Layout: the XDVDFS volume descriptor ("MICROSOFT*XBOX*MEDIA") is sector 32
of the game partition; directories are binary trees of 4-byte aligned
entries (left, right, sector, size, attributes, name length, name).
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

SECTOR = 2048
MAGIC = b'MICROSOFT*XBOX*MEDIA'
# Game partition offsets: XGD3, XGD2, XGD1, plain XISO.
PARTITIONS = (0x2080000, 0xFD90000, 0x18300000, 0)


def find_partition(file) -> int:
    for base in PARTITIONS:
        file.seek(base + 32 * SECTOR)
        if file.read(len(MAGIC)) == MAGIC:
            return base
    raise ValueError('Not an Xbox disc image (no XDVDFS volume descriptor)')


def walk(file, base: int, sector: int, size: int, prefix: str = ''):
    if size == 0:
        return
    file.seek(base + sector * SECTOR)
    table = file.read(size)
    seen = set()

    def node(offset: int):
        if offset in seen or offset + 14 > len(table):
            return
        seen.add(offset)
        left, right, start, length, attributes, name_length = struct.unpack_from('<HHIIBB', table, offset)
        if left == 0xFFFF and right == 0xFFFF:  # padding entry
            return
        name = table[offset + 14:offset + 14 + name_length].decode('ascii', errors='replace')
        if left:
            yield from node(left * 4)
        path = prefix + name
        if attributes & 0x10:
            yield path + '/', start, length, True
            yield from walk(file, base, start, length, path + '/')
        else:
            yield path, start, length, False
        if right:
            yield from node(right * 4)

    yield from node(0)


def entries(image: Path):
    with image.open('rb') as file:
        base = find_partition(file)
        file.seek(base + 32 * SECTOR + len(MAGIC))
        root_sector, root_size = struct.unpack('<II', file.read(8))
        return base, list(walk(file, base, root_sector, root_size))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('image', type=Path)
    parser.add_argument('output', type=Path, nargs='?')
    parser.add_argument('names', nargs='*')
    args = parser.parse_args()
    base, items = entries(args.image)
    if not args.output:
        print(f'partition 0x{base:X}')
        for path, sector, size, directory in items:
            print(f'{"<dir>" if directory else size:>12}  {path}')
        return
    with args.image.open('rb') as file:
        for path, sector, size, directory in items:
            if directory or (args.names and path not in args.names):
                continue
            target = args.output / path
            target.parent.mkdir(parents=True, exist_ok=True)
            file.seek(base + sector * SECTOR)
            target.write_bytes(file.read(size))
            print(path)


if __name__ == '__main__':
    main()
