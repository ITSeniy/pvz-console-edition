"""Record reproducible source/binary evidence for cross-platform implementation research.

Requires analyze_cross_platform.py outputs and the existing pvz_inspect image dump.
Addresses belong to the supplied image only; no executable is run or patched.
"""
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET

from analyze_cross_platform import elf32, save_json, sha

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'research/cross-platform'


def animation_sources():
    source = OUT / 'text/ps3/compressed.pak'
    records, failures = [], []
    for path in sorted(source.rglob('*.reanim')):
        try:
            tree = ET.fromstring('<root>' + path.read_text(encoding='utf-8') + '</root>')
            tracks = tree.findall('track')
            records.append({'source': path.relative_to(OUT).as_posix(),
                'name': path.relative_to(source).as_posix(), 'fps': tree.findtext('fps'),
                'tracks': len(tracks), 'frames': max((len(t.findall('t')) for t in tracks), default=0),
                'animations': [t.findtext('name') for t in tracks if (t.findtext('name') or '').startswith('anim_')],
                'image_ids': sorted({i.text for i in tree.iter('i') if i.text})})
        except ET.ParseError as e:
            failures.append({'path': str(path), 'error': str(e)})
    save_json(OUT / 'ps3-reanimation-sources.json', records)
    save_json(OUT / 'ps3-reanimation-parse-errors.json', failures)


def challenge_tables():
    android_path = ROOT / 'Chinese Android TV Image/lib/armeabi/libGameMain.so'
    data, sections, symbols = elf32(android_path)
    def read(addr, length):
        sec = next(s for s in sections if s['type'] != 8 and s['address'] <= addr < s['address'] + s['size'])
        off = sec['offset'] + addr - sec['address']
        return data[off:off + length]
    sym = next(s for s in symbols if s['name'] == 'gChallengeDefs')
    def entries(raw, endian, reader):
        result = []
        for off in range(0, len(raw), 24):
            mode, thumb, page, row, col, ptr = struct.unpack_from(endian + '6I', raw, off)
            try:
                name = reader(ptr, 160).split(b'\0')[0].decode('ascii')
            except (StopIteration, UnicodeDecodeError):
                break
            if mode > 100 or thumb > 30 or page > 5 or row > 30 or col > 30 or (name and not re.fullmatch(r'\[[A-Za-z_0-9 ]+\]', name)):
                break
            result.append({'index': off // 24, 'mode': mode, 'thumbnail': thumb, 'page': page,
                           'row': row, 'column': col, 'name': name})
        return result
    android = entries(read(sym['value'], sym['size']), '<', read)
    if len(android) * 24 != sym['size']:
        raise ValueError('Android gChallengeDefs parse is incomplete')
    xbox_path = ROOT / '.cache/pvz-image.bin'
    if not xbox_path.exists():
        save_json(OUT / 'challenge-tables.json', {'android': android, 'xbox_unavailable': True})
        return
    xbox = xbox_path.read_bytes()
    key = xbox.find(b'[SURVIVAL_DAY_NORMAL]\0')
    if key < 0:
        raise ValueError('Cannot locate Xbox challenge localization key')
    pattern = struct.pack('>6I', 2, 0, 0, 0, 0, 0x82000000 + key)
    hits = [m.start() for m in re.finditer(re.escape(pattern), xbox)]
    if len(hits) != 1:
        raise ValueError('Ambiguous Xbox challenge table')
    off = hits[0]
    xx = entries(xbox[off:off + sym['size'] + 240], '>', lambda addr, n: xbox[addr - 0x82000000:addr - 0x82000000 + n])
    ax, bx = {x['index']: x for x in android}, {x['index']: x for x in xx}
    diffs = {str(index): {'android': ax[index], 'xbox': bx[index]} for index in ax.keys() & bx.keys() if ax[index] != bx[index]}
    save_json(OUT / 'challenge-tables.json', {'android_address': hex(sym['value']),
        'xbox_address': hex(0x82000000 + off), 'xbox_dump_sha256': sha(xbox),
        'android_sha256': sha(data), 'android': android, 'xbox': xx,
        'only_android': sorted(ax.keys() - bx.keys()), 'only_xbox': sorted(bx.keys() - ax.keys()),
        'changed': diffs})


def vita_typeinfo():
    segments = [(0x81000000, (OUT / 'vita-segment-0.bin').read_bytes()),
                (0x8144b000, (OUT / 'vita-segment-1.bin').read_bytes())]
    raw = segments[0][1]
    names = ['BaseGamepadControls', 'GamepadControls', 'BaseGamepadControlsTouch', 'GamepadControlsTouch',
             'TreeOfWisdomControlsTouch', 'ZenGardenControlsTouch', 'HelpTouchDialog', 'StoreScreenTouch',
             'ZombatarScreen', 'ZombatarWidget']
    found = []
    for name in names:
        needle = str(len(name)).encode() + name.encode() + b'\0'
        for match in re.finditer(re.escape(needle), raw):
            name_addr = 0x81000000 + match.start()
            for base, seg in segments:
                for ref in re.finditer(re.escape(struct.pack('<I', name_addr)), seg):
                    if ref.start() % 4:
                        continue
                    rtti = base + ref.start() - 4
                    if not 0x81000000 <= struct.unpack_from('<I', seg, ref.start() - 4)[0] < 0x81584820:
                        continue
                    vtables = []
                    for vb, vs in segments:
                        for vr in re.finditer(re.escape(struct.pack('<I', rtti)), vs):
                            off = vr.start()
                            if off % 4 or off < 4 or struct.unpack_from('<i', vs, off - 4)[0] != 0:
                                continue
                            slots = []
                            for s in range(100):
                                ptr = struct.unpack_from('<I', vs, off + 4 + 4 * s)[0]
                                if not 0x81000000 <= ptr < 0x813b0000:
                                    break
                                slots.append({'slot': s, 'pointer': hex(ptr), 'code_address': hex(ptr & ~1), 'thumb': bool(ptr & 1)})
                            if slots:
                                vtables.append({'address': hex(vb + off + 4), 'slots': slots})
                    if vtables:
                        found.append({'class': name, 'name_address': hex(name_addr), 'typeinfo_address': hex(rtti),
                                      'vtables': vtables})
    save_json(OUT / 'vita-class-vtables.json', found)


if __name__ == '__main__':
    animation_sources()
    challenge_tables()
    vita_typeinfo()
    print('Recorded animation sources, challenge tables and Vita class vtables.')
