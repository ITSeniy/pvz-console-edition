"""Import raw PvZ test settings into an existing modern Xenia Canary GPD.

Format: xenia-canary/xenia-canary commit 0b041cd2a, kernel/xam/xdbf.
Keeps other entries unchanged, backs up the GPD and replaces it atomically.
"""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess

SIZES = {'63E83FFD': 160, '63E83FFE': 1000, '63E83FFF': 850}
HEADER = struct.Struct('>6I')
ENTRY = struct.Struct('>HQII')
FREE = struct.Struct('>II')
SETTING = struct.Struct('>IIB7xII')
TARGETS = {(3, int(name, 16)) for name in SIZES}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def parse_gpd(data):
    if len(data) < HEADER.size:
        raise ValueError('Truncated XDBF header')
    magic, version, capacity, used, free_capacity, free_used = HEADER.unpack_from(data)
    if magic != 0x58444246 or version != 0x10000:
        raise ValueError('Unsupported XDBF signature or version')
    if not 0 < used <= capacity <= 65536 or not 0 < free_used <= free_capacity <= 65536:
        raise ValueError('Invalid XDBF table counts')
    free_start = HEADER.size + capacity * ENTRY.size
    data_start = free_start + free_capacity * FREE.size
    if data_start > len(data):
        raise ValueError('Truncated XDBF tables')
    data_size = len(data) - data_start
    entries = {}
    occupied = []
    for i in range(used):
        section, identifier, offset, size = ENTRY.unpack_from(data, HEADER.size + i * ENTRY.size)
        key = section, identifier
        if key in entries or offset + size > data_size:
            raise ValueError('Duplicate entry or out-of-bounds XDBF data')
        entries[key] = (offset, size, data[data_start + offset:data_start + offset + size])
        if size:
            occupied.append((offset, offset + size))
    occupied.sort()
    if any(left[1] > right[0] for left, right in zip(occupied, occupied[1:])):
        raise ValueError('Overlapping XDBF entries')
    free = [FREE.unpack_from(data, free_start + i * FREE.size) for i in range(free_used)]
    if free[-1] != (data_size, 0xFFFFFFFF - data_size):
        raise ValueError('Unsupported XDBF terminal free-space marker')
    holes = sorted(free[:-1])
    for i, (offset, size) in enumerate(holes):
        if not size or offset + size > data_size:
            raise ValueError('Invalid XDBF free-space block')
        if i and holes[i - 1][0] + holes[i - 1][1] > offset:
            raise ValueError('Overlapping XDBF free-space blocks')
        if any(offset < end and offset + size > start for start, end in occupied):
            raise ValueError('XDBF free space overlaps an entry')
    return capacity, free_capacity, data_start, entries, holes


def read_payloads(profile):
    payloads = {name: (profile / name).read_bytes() for name in SIZES}
    for name, size in SIZES.items():
        if len(payloads[name]) != size:
            raise ValueError(f'{name}: expected {size} raw bytes, without a Xenia header')
    progress = payloads['63E83FFF']
    if struct.unpack_from('>I', progress)[0] != 19:
        raise ValueError('Only PvZ profile version 19 is supported')
    if not struct.unpack_from('>I', progress, 0xC)[0] & 0x8001:
        raise ValueError('Expected the completed-Adventure test profile')
    trophies = sum(value > 0 for value in struct.unpack_from('>100i', progress, 0x10)[15:35])
    if trophies < 17:
        raise ValueError('Test profile does not unlock all mini-game rows')
    return payloads, trophies


def convert(original, payloads):
    capacity, free_capacity, data_start, entries, holes = parse_gpd(original)
    unchanged = {key: value for key, value in entries.items() if key not in TARGETS}
    for key in TARGETS:
        old = entries.pop(key, None)
        if old and old[1]:
            holes.append((old[0], old[1]))
    result = bytearray(original)
    for name, payload in payloads.items():
        identifier = int(name, 16)
        serialized = SETTING.pack(identifier, 0, 6, len(payload), 0) + payload
        offset = len(result) - data_start
        entries[3, identifier] = (offset, len(serialized), serialized)
        result.extend(serialized)
    # Merge adjacent holes; retain the original data area and unrelated offsets.
    merged = []
    for offset, size in sorted(holes):
        if merged and merged[-1][0] + merged[-1][1] == offset:
            merged[-1] = (merged[-1][0], merged[-1][1] + size)
        else:
            merged.append((offset, size))
    data_size = len(result) - data_start
    free = merged + [(data_size, 0xFFFFFFFF - data_size)]
    if len(entries) > capacity or len(free) > free_capacity:
        raise ValueError('XDBF table capacity exhausted; no files changed')
    HEADER.pack_into(result, 0, 0x58444246, 0x10000, capacity, len(entries), free_capacity, len(free))
    result[HEADER.size:data_start] = bytes(data_start - HEADER.size)
    for i, ((section, identifier), (offset, size, _)) in enumerate(sorted(entries.items())):
        ENTRY.pack_into(result, HEADER.size + i * ENTRY.size, section, identifier, offset, size)
    free_start = HEADER.size + capacity * ENTRY.size
    for i, block in enumerate(free):
        FREE.pack_into(result, free_start + i * FREE.size, *block)
    result = bytes(result)
    parsed = parse_gpd(result)[3]
    if {key: value for key, value in parsed.items() if key not in TARGETS} != unchanged:
        raise RuntimeError('Unrelated GPD entries changed')
    for name, payload in payloads.items():
        serialized = parsed[3, int(name, 16)][2]
        if SETTING.unpack_from(serialized) != (int(name, 16), 0, 6, len(payload), 0):
            raise RuntimeError('Invalid serialized binary setting')
        if serialized[SETTING.size:] != payload:
            raise RuntimeError('Imported payload mismatch')
    return result, len(unchanged)


def require_xenia_closed():
    if os.name == 'nt':
        check = subprocess.run([
            'powershell.exe', '-NoProfile', '-NonInteractive', '-Command',
            "if (Get-Process | Where-Object { $_.ProcessName -like '*xenia*' }) { exit 7 }"
        ], capture_output=True, timeout=20)
        if check.returncode:
            raise RuntimeError('Close Xenia before importing; no profile was replaced')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profile', type=Path, help='Raw settings folder, ending in profile/User')
    parser.add_argument('gpd', type=Path, help='Existing 584109FF.gpd of the active Xenia profile')
    parser.add_argument('--backup-root', type=Path, help='Required when writing')
    parser.add_argument('--report', type=Path)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    profile, gpd = args.profile.resolve(), args.gpd.resolve()
    if gpd.name.lower() != '584109ff.gpd':
        parser.error('Destination must be the PvZ title GPD, 584109FF.gpd')
    if not args.dry_run and args.backup_root is None:
        parser.error('--backup-root is required when writing')
    payloads, trophies = read_payloads(profile)
    original = gpd.read_bytes()
    converted, preserved = convert(original, payloads)
    report = {
        'source_profile': str(profile), 'destination_gpd': str(gpd),
        'dry_run': args.dry_run, 'trophies': trophies, 'available_minigame_rows': 20,
        'preserved_entries': preserved, 'gpd_before_bytes': len(original),
        'gpd_after_bytes': len(converted), 'gpd_before_sha256': sha(original),
        'gpd_after_sha256': sha(converted),
        'source_sha256': {name: sha(data) for name, data in payloads.items()},
        'validation': 'GPD bounds, free space, unrelated entries and all three payloads verified',
        'in_game_verified': False,
    }
    if not args.dry_run:
        require_xenia_closed()
        stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
        backup = args.backup_root.resolve() / f'pvz-test-{stamp}'
        backup.mkdir(parents=True, exist_ok=False)
        backup_file = backup / gpd.name
        with backup_file.open('xb') as out:
            out.write(original)
            out.flush()
            os.fsync(out.fileno())
        if backup_file.read_bytes() != original:
            raise RuntimeError('Backup verification failed')
        report['backup_gpd'] = str(backup_file)
        temporary = gpd.with_name(f'.{gpd.name}.pvz-import-{stamp}.tmp')
        with temporary.open('xb') as out:
            out.write(converted)
            out.flush()
            os.fsync(out.fileno())
        if temporary.read_bytes() != converted:
            raise RuntimeError('Staged GPD verification failed')
        require_xenia_closed()
        if gpd.read_bytes() != original or any((profile / name).read_bytes() != data
                                               for name, data in payloads.items()):
            raise RuntimeError('Profile changed during import; destination was not replaced')
        os.replace(temporary, gpd)
        if gpd.read_bytes() != converted:
            raise RuntimeError('Destination readback mismatch; restore the recorded backup')
        report['readback_verified'] = True
        (backup / 'import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        (backup / 'RESTORE.txt').write_text(
            'Close Xenia, then copy 584109FF.gpd from this folder over:\n' + str(gpd) + '\n',
            encoding='utf-8')
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
