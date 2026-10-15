"""Build a tester profile at an Adventure level from a fresh game-written profile.

The source is a first-run profile the game wrote itself (level 0, no coins).
Only the Adventure level is changed: in the first playthrough the game derives
unlocked plants and Dave's lessons from it. The result is a user data folder
for run.ps1 -UserDataRoot.

    python scripts/make_level_save.py runtime-data/smoke/<fresh>/user runtime-data/level-saves/1-10 10
"""
import argparse
import shutil
import struct
from pathlib import Path

PROFILE = Path('584109FF/profile/User')
MAIN = '63E83FFF'  # [version 19, Adventure level, coins / 10, ...]


def make(source: Path, destination: Path, level: int) -> None:
    blob = bytearray((source / PROFILE / MAIN).read_bytes())
    version, current, coins = struct.unpack_from('>3I', blob, 0)
    if len(blob) != 850 or version != 19:
        raise ValueError(f'Unexpected profile layout: {len(blob)} bytes, version {version}')
    if current > 1 or coins:
        raise ValueError('Source must be a fresh profile (level 0 or 1, no coins)')
    if destination.exists():
        raise FileExistsError(destination)
    shutil.copytree(source / PROFILE, destination / PROFILE)
    struct.pack_into('>I', blob, 4, level)
    (destination / PROFILE / MAIN).write_bytes(bytes(blob))
    print(f'{destination}: Adventure level {level} (from {current})')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('level', type=int, help='1-1 = 1, 1-10 = 10, 5-10 = 50')
    args = parser.parse_args()
    if not 1 <= args.level <= 50:
        parser.error('level must be 1..50')
    make(args.source, args.destination, args.level)
