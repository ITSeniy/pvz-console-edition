"""Extract a read-only LIVE/PIRS STFS package, checking each data block's SHA-1.

Layout reference: rexglue-sdk's stfs_xbox.h and stfs_container_device.cpp
(Xenia / ReXGlue, BSD-3-Clause). This deliberately rejects writable STFS/SVOD.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def extract(package: Path, destination: Path) -> dict:
    data = package.read_bytes()
    if data[:4] not in (b"LIVE", b"PIRS"):
        raise ValueError("Expected a read-only LIVE/PIRS package")
    if len(data) < 0xA000:
        raise ValueError("Truncated STFS header")
    be = lambda offset, size: int.from_bytes(data[offset:offset + size], "big")
    le = lambda offset, size: int.from_bytes(data[offset:offset + size], "little")
    if be(0x3A9, 4) != 0 or data[0x379] != 0x24 or not data[0x37B] & 1:
        raise ValueError("Only read-only STFS volumes are supported")
    header_size = (be(0x340, 4) + 0xFFF) & ~0xFFF
    total_blocks = be(0x395, 4)
    if total_blocks > 170 ** 2:
        raise ValueError("Level-2 packages are not yet supported")

    def offset(block: int) -> int:
        if not 0 <= block < total_blocks:
            raise ValueError(f"Block outside volume: {block}")
        physical = block + (block + 170) // 170
        if block >= 170:
            physical += (block + 170 ** 2) // (170 ** 2)
        return header_size + physical * 0x1000

    def read_block(block: int) -> tuple[bytes, int]:
        start = offset(block)
        payload = data[start:start + 0x1000]
        hash_block = 0 if block < 170 else (block // 170) * 171 + 1
        record = header_size + hash_block * 0x1000 + (block % 170) * 0x18
        if len(payload) != 0x1000 or record + 0x18 > len(data):
            raise ValueError(f"Truncated block {block}")
        if hashlib.sha1(payload).digest() != data[record:record + 20]:
            raise ValueError(f"SHA-1 mismatch in block {block}")
        return payload, be(record + 21, 3)

    paths: list[Path] = []
    files = []
    table_block = le(0x37E, 3)
    table_seen = set()
    destination = destination.resolve()
    for _ in range(le(0x37C, 2)):
        if table_block in table_seen:
            raise ValueError("Cycle in file table")
        table_seen.add(table_block)
        table, table_block = read_block(table_block)
        for at in range(0, 0x1000, 0x40):
            entry = table[at:at + 0x40]
            if not entry[0]:
                break
            name = entry[:entry[40] & 0x3F].decode("utf-8")
            if not name or name in (".", "..") or any(c in name for c in '\\/:\x00'):
                raise ValueError(f"Unsafe filename: {name!r}")
            parent = int.from_bytes(entry[50:52], "big")
            relative = Path(name) if parent == 0xFFFF else paths[parent] / name
            target = (destination / relative).resolve()
            if not target.is_relative_to(destination):
                raise ValueError("Path escapes destination")
            paths.append(relative)
            if entry[40] & 0x80:
                target.mkdir(parents=True, exist_ok=True)
                continue
            size = int.from_bytes(entry[52:56], "big")
            block = int.from_bytes(entry[47:50], "little")
            remaining, chunks, seen = size, [], set()
            while remaining:
                if block in seen or block == 0xFFFFFF:
                    raise ValueError(f"Broken chain: {relative}")
                seen.add(block)
                chunk, block = read_block(block)
                chunks.append(chunk[:remaining])
                remaining -= min(remaining, 0x1000)
            content = b"".join(chunks)
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.exists() and target.read_bytes() != content:
                raise FileExistsError(f"Refusing to overwrite different file: {target}")
            target.write_bytes(content)
            files.append({"path": relative.as_posix(), "size": size,
                          "sha256": hashlib.sha256(content).hexdigest()})
    return {"package": str(package), "package_sha256": hashlib.sha256(data).hexdigest(),
            "title_id": f"{be(0x360, 4):08X}", "files": files}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    report = extract(args.package, args.destination)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2), encoding="utf-8")
    for item in report["files"]:
        print(f'{item["size"]:10d}  {item["path"]}')
