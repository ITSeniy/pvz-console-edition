"""Unlock the last five mini-games in a RexGlue test profile, with a backup.

Native ChallengeScreen unlocks row N when N < mini-game trophies + 3.
There is no independent saved unlock flag: mark challenge IDs 30..34 won.
PlayerInfo::SyncDetails (821D9030), version 19, serializes 100 BE records
at offset 0x10 of setting 63E83FFF. Existing positive records are preserved.
"""
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import struct

SIZES = {'63E83FFD': 160, '63E83FFE': 1000, '63E83FFF': 850}
NAMES = ['Last Stand', 'Zombotany 2', 'Wall-nut Bowling 2', 'Pogo Party', "Dr. Zomboss's Revenge"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('user_data_root', type=Path)
    args = p.parse_args()
    root = args.user_data_root.resolve()
    profile = root / '584109FF' / 'profile' / 'User'
    files = {name: (profile / name).read_bytes() for name in SIZES}
    for name, size in SIZES.items():
        if len(files[name]) != size:
            raise ValueError(f'{name}: expected a raw RexGlue setting of {size} bytes')
    before = files['63E83FFF']
    if struct.unpack_from('>I', before)[0] != 19:
        raise ValueError('Only profile version 19 is supported')
    records = list(struct.unpack_from('>100i', before, 0x10))
    if not struct.unpack_from('>I', before, 0xC)[0] & 0x8001:
        raise ValueError('This profile has not finished Adventure; use the imported mini-games test profile')
    after = bytearray(before)
    changes = []
    for challenge, title in zip(range(30, 35), NAMES):
        if records[challenge] <= 0:
            offset = 0x10 + challenge * 4
            struct.pack_into('>i', after, offset, 1)
            changes.append({'challenge_id': challenge, 'title': title,
                            'offset': offset, 'before': records[challenge], 'after': 1})
    new_records = struct.unpack_from('>100i', after, 0x10)
    trophies_before = sum(value > 0 for value in records[15:35])
    trophies_after = sum(value > 0 for value in new_records[15:35])
    if trophies_after < 17:
        raise ValueError('The last five wins are insufficient for this profile; no files changed')
    report = {'user_data_root': str(root), 'trophies_before': trophies_before,
              'trophies_after': trophies_after, 'unlocked_minigames': 20,
              'changes': changes, 'before_sha256': sha(before), 'after_sha256': sha(after)}
    if changes:
        stamp = datetime.now().strftime('%Y%m%d-%H%M%S-%f')
        backup = root.parent / 'save-backups' / stamp
        backup.mkdir(parents=True)
        for name, data in files.items():
            (backup / name).write_bytes(data)
        report['backup_directory'] = str(backup)
        report['backup_sha256'] = {name: sha(data) for name, data in files.items()}
        (backup / 'unlock.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
        # Do not overwrite a profile updated since it was read.
        if any((profile / name).read_bytes() != data for name, data in files.items()):
            raise RuntimeError('Profile changed during preparation; close the game and retry')
        temporary = profile / '63E83FFF.unlock-tmp'
        temporary.write_bytes(after)
        temporary.replace(profile / '63E83FFF')
        assert (profile / '63E83FFF').read_bytes() == after
    (root.parent / 'unlock-minigames.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
