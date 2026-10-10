"""Import the three PvZ Xenia binary settings into a NEW RexGlue user root."""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path, help='Folder containing the three settings, or a ZIP')
parser.add_argument('destination', type=Path, help='New, separate user data root')
args = parser.parse_args()
sizes = {'63E83FFD': 160, '63E83FFE': 1000, '63E83FFF': 850}
payloads = {}
inventory = []
if args.destination.exists():
    parser.error('Destination already exists; choose a new test profile.')
archive = zipfile.ZipFile(args.source) if args.source.is_file() else None
try:
    for name, size in sizes.items():
        data = archive.read(name) if archive else (args.source / name).read_bytes()
        if len(data) != size + 24:
            raise ValueError(f'{name}: unexpected serialized setting size')
        setting_id, = struct.unpack_from('>I', data, 0)
        payload_size, = struct.unpack_from('>I', data, 16)
        if setting_id != int(name, 16) or data[8] != 6 or payload_size != size:
            raise ValueError(f'{name}: invalid Xenia binary setting header')
        payloads[name] = data[24:]
        inventory.append({'file': name, 'source_bytes': len(data), 'imported_bytes': size,
                          'source_sha256': hashlib.sha256(data).hexdigest(),
                          'imported_sha256': hashlib.sha256(data[24:]).hexdigest()})
finally:
    if archive:
        archive.close()
version, = struct.unpack_from('>I', payloads['63E83FFF'])
if version != 19:
    raise ValueError(f'Unexpected PvZ profile version: {version}')
profile = args.destination / '584109FF' / 'profile' / 'User'
profile.mkdir(parents=True)
for name, data in payloads.items():
    (profile / name).write_bytes(data)
report = {'source': str(args.source.resolve()), 'user_data_root': str(args.destination.resolve()),
          'setting_directory': str(profile.resolve()), 'header_bytes_removed': 24,
          'files': inventory, 'original_source_preserved': True}
(args.destination.parent / 'import.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
