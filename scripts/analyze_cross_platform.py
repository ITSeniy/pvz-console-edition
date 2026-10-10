"""Read-only comparison of supplied PvZ Xbox, PS3, Vita and Android TV images.

Outputs inventories, hashes, localization differences and ARM symbol evidence.
Optional QuickBMS decodes XCTD into the research directory; originals stay intact.
Run from the project root: python scripts/analyze_cross_platform.py --quickbms PATH
Pillow and Capstone are optional (the project's tools/python is searched).
"""
import argparse
from collections import Counter
import hashlib
import io
import itertools
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib


def sha(data):
    return hashlib.sha256(data).hexdigest()


def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def pak_entries(path, sony=False):
    data = path.read_bytes()
    if data[:8] != bytes.fromhex('c04ac0ba00000000'):
        raise ValueError(f'Unsupported PAK header: {path}')
    pos, entries = 8, []
    while pos < len(data) and data[pos] != 0x80:
        flags, width = data[pos:pos + 2]
        if flags or not width:
            raise ValueError(f'Invalid PAK entry at {pos}: {path}')
        pos += 2
        name = data[pos:pos + width].decode('ascii').replace('\\', '/').lower()
        pos += width
        size = struct.unpack_from('<I', data, pos)[0]
        expanded = struct.unpack_from('<I', data, pos + 4)[0] if sony else 0
        pos += 16 if sony else 12
        entries.append((name, size, expanded))
    if pos >= len(data):
        raise ValueError(f'Missing table terminator: {path}')
    pos += 1
    result = []
    for name, size, expanded in entries:
        pad = struct.unpack_from('<H', data, pos)[0]
        pos += 2 + pad
        offset = pos
        payload = data[pos:pos + size]
        if len(payload) != size:
            raise ValueError(f'Truncated payload: {path}:{name}')
        pos += size
        raw = zlib.decompress(payload) if expanded else payload
        if expanded and len(raw) != expanded:
            raise ValueError(f'Wrong expanded size: {path}:{name}')
        result.append((name, raw, {'container': str(path), 'offset': offset,
                                  'stored_size': size, 'expanded_size': expanded}))
    if pos != len(data):
        raise ValueError(f'Unconsumed PAK bytes: {path}: {pos}/{len(data)}')
    return result


def decode_xctd(path, out, quickbms):
    data = path.read_bytes()
    magic, version, reserved, crc, flags = struct.unpack_from('>IHHII', data)
    if magic != 0x0ff512ed or version != 0x100:
        raise ValueError('Unsupported XCTD header')
    count = (flags >> 6) & 0xffff
    bits = 32 if (flags >> 22) & 3 else 20
    if bits != 32:
        raise ValueError('This supplied-image workflow expects 32-bit XCTD table entries')
    sizes = struct.unpack_from('>' + str(count) + 'I', data, 16)
    expected = sum(sizes)
    out.mkdir(parents=True, exist_ok=True)
    target = out / path.name
    provenance = target.with_suffix('.pak.meta.json')
    source_sha = sha(data)
    cached = json.loads(provenance.read_text(encoding='utf-8')) if provenance.exists() else {}
    valid_cache = (target.exists() and target.stat().st_size == expected and
                   cached.get('source_sha256') == source_sha and
                   cached.get('decoded_sha256') == sha(target.read_bytes()))
    if not valid_cache:
        if not quickbms:
            raise ValueError(f'Need --quickbms to decode {path}')
        script = out / (path.stem + '-decode.bms')
        script.write_text(f'comtype xmemdecompress\nget ZSIZE asize\nclog "{path.name}" 0 ZSIZE {expected}\n')
        subprocess.run([str(quickbms.resolve()), '-o', str(script.resolve()),
                        str(path.resolve()), str(out.resolve())], check=True, capture_output=True)
    if target.stat().st_size != expected or target.open('rb').read(8) != bytes.fromhex('c04ac0ba00000000'):
        raise ValueError(f'Invalid decoded PAK: {path}')
    metadata = {'source': str(path), 'source_sha256': source_sha, 'flags': hex(flags),
                'segments': count, 'decoded_size': expected, 'decoded_sha256': sha(target.read_bytes()),
                'crc_field': hex(crc), 'crc_verified': False}
    save_json(provenance, metadata)
    return target, metadata


def decode_text(raw):
    if raw.startswith(b'\xfe\xff'):
        return raw.decode('utf-16').lstrip('\ufeff')
    if raw.startswith(b'\xff\xfe'):
        return raw.decode('utf-16').lstrip('\ufeff')
    if raw[:2] in (b'\x00[', b'\x00<'):
        return raw.decode('utf-16-be')
    if raw[:2] in (b'[\x00', b'<\x00'):
        return raw.decode('utf-16-le')
    for encoding in ('utf-8-sig', 'gb18030', 'cp1252'):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            pass
    return None


def lawn_strings(raw):
    txt = decode_text(raw)
    if txt is None:
        return {}
    txt = txt.replace('\r', '')
    return {m[1]: m[2].strip() for m in re.finditer(
        r'^\[([^\]\n]+)\]\n(.*?)(?=^\[[^\]\n]+\]\n|\Z)', txt, re.M | re.S)}


def xbox_dds(raw):
    """Linear DXT5 PTX used by this Xbox image: padded rows, 16-bit swap."""
    w, h, stride, fmt = struct.unpack('>4I', raw[-16:])
    if fmt != 0x5400201a or not 0 < w <= 32768 or not 0 < h <= 32768:
        raise ValueError('Unsupported PTX format/dimensions')
    row = ((w + 3) // 4) * 16
    rows = (h + 3) // 4
    if stride < row or len(raw) < rows * stride + 16:
        raise ValueError('Invalid PTX stride/length')
    blocks = bytearray()
    for y in range(rows):
        src = raw[y * stride:y * stride + row]
        swapped = bytearray(len(src))
        swapped[::2], swapped[1::2] = src[1::2], src[::2]
        blocks += swapped
    header = struct.pack('<7I', 124, 0x81007, h, w, len(blocks), 0, 0)
    header += b'\0' * 44
    header += struct.pack('<II4s5I', 32, 4, b'DXT5', 0, 0, 0, 0, 0)
    header += struct.pack('<5I', 0x1000, 0, 0, 0, 0)
    return b'DDS ' + header + blocks


def elf32(path):
    data = path.read_bytes()
    if data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected ELF32 little endian')
    off = struct.unpack_from('<I', data, 32)[0]
    ents, num, name_idx = struct.unpack_from('<HHH', data, 46)
    sections = [struct.unpack_from('<10I', data, off + i * ents) for i in range(num)]
    ns = sections[name_idx]
    names = data[ns[4]:ns[4] + ns[5]]
    def string(table, index):
        return table[index:].split(b'\0')[0].decode('utf-8', errors='replace')
    syms = []
    for s in sections:
        if s[1] not in (2, 11):
            continue
        st = sections[s[6]]
        strings = data[st[4]:st[4] + st[5]]
        for i in range(0, s[5], s[9]):
            n, v, size, info, other, idx = struct.unpack_from('<IIIBBH', data, s[4] + i)
            syms.append({'name': string(strings, n), 'value': v, 'size': size,
                         'type': info & 15, 'binding': info >> 4, 'section': idx})
    return data, [{'name': string(names, s[0]), 'address': s[3], 'offset': s[4],
                   'size': s[5], 'type': s[1]} for s in sections], syms


def symbol_analysis(root, output):
    path = root / 'Chinese Android TV Image/lib/armeabi/libGameMain.so'
    data, sections, symbols = elf32(path)
    filt = root / 'research/rexglue-source/tools/binutils/powerpc-none-elf-c++filt.exe'
    if filt.exists():
        result = subprocess.run([str(filt)], input='\n'.join(s['name'] for s in symbols),
                                text=True, capture_output=True, check=True)
        demangled = result.stdout.splitlines()
        if len(demangled) != len(symbols):
            raise ValueError('Demangler result count mismatch')
        for s, dem in zip(symbols, demangled):
            s['demangled'] = dem
    save_json(output / 'android-symbols.json', symbols)
    defined = [s for s in symbols if s['section'] and s['type'] == 2 and s['size']]
    selected = [s for s in defined if re.search(
        r'GamepadControls|HeavyWeapon|SaveGame|SyncBoard|ChallengeScreen|SeedChooserScreen|'
        r'Board::Mouse|SeedPacket::|Coin::Collect|DrawSeedPacket|LawnApp::.*(Save|User|Config)|'
        r'Challenge::.*(Mouse|Beghouled|Whack|Zombiquarium)', s.get('demangled', s['name']))]
    save_json(output / 'android-relevant-symbols.json', selected)
    try:
        from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_ARM
    except ImportError:
        Cs = None
    if Cs:
        addr_names = {s['value'] & ~1: s.get('demangled', s['name']) for s in defined}
        disasm = []
        exact = ['Board::MouseDown(int, int, int)', 'Board::MouseMove(int, int)',
                 'SeedChooserScreen::MouseDown(int, int, int)', 'StoreScreen::MouseDown(int, int, int)',
                 'Challenge::HeavyWeaponButtonDown(int, int, unsigned int)',
                 'Challenge::HeavyWeaponUpdate()', 'Challenge::HeavyWeaponPacketClicked(SeedPacket*)',
                 'GamepadControls::IsMagnetized(CoinType)', 'SeedPacket::CanPickUp()',
                 'GamepadControls::GetSeedBank()', 'BaseGamepadControls::GetCursorPosition(float&, float&)',
                 'BaseGamepadControls::GetGamepadVelocity(float&, float&)',
                 'ChallengeScreen::MoreTrophiesNeeded(int)', 'SyncBoard(SaveGameContext*, Board*)']
        for sym in defined:
            if sym.get('demangled') not in exact:
                continue
            addr = sym['value'] & ~1
            sec = next(s for s in sections if s['type'] != 8 and s['address'] <= addr < s['address'] + s['size'])
            off = sec['offset'] + addr - sec['address']
            cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB if sym['value'] & 1 else CS_MODE_ARM)
            disasm.append(f"\n{sym['demangled']} value=0x{sym['value']:08x} size={sym['size']}")
            for ins in cs.disasm(data[off:off + sym['size']], addr):
                annotation = ''
                if ins.mnemonic.startswith('b') and ins.op_str.startswith('#0x'):
                    annotation = addr_names.get(int(ins.op_str[1:], 16), '')
                disasm.append(f'{ins.address:08x}  {ins.mnemonic:10s} {ins.op_str:32s} {annotation}')
        (output / 'android-functions.txt').write_text('\n'.join(disasm) + '\n', encoding='utf-8')
    tables = [s for s in symbols if s['section'] and s['type'] == 1 and re.search(
        r'Definition|Definitions|Defs|vtable for (Board|GamepadControls|Challenge|Seed|Coin)', s.get('demangled', s['name']))]
    return {'source': str(path.relative_to(root)), 'sha256': sha(data), 'symbols': len(symbols),
            'defined_functions': len(defined), 'relevant_functions': len(selected),
            'definition_and_vtable_symbols': tables, 'sections': sections}


def vita_segments(root, output):
    path = root / 'PSVita Image/eboot.bin'
    data = path.read_bytes()
    elf = struct.unpack_from('<Q', data, 0x40)[0]
    ph = struct.unpack_from('<Q', data, 0x48)[0]
    seg = struct.unpack_from('<Q', data, 0x58)[0]
    num = struct.unpack_from('<H', data, elf + 44)[0]
    entries = []
    for i in range(num):
        kind, off, va, pa, filesz, memsz, flags, align = struct.unpack_from('<8I', data, ph + 32 * i)
        offset, length, compressed, encrypted = struct.unpack_from('<4Q', data, seg + 32 * i)
        item = {'index': i, 'virtual_address': hex(va), 'self_offset': offset, 'size': length,
                'elf_file_size': filesz, 'compression_field': compressed, 'encryption_field': encrypted}
        if encrypted == 2 and compressed == 1:
            raw = data[offset:offset + length]
            if len(raw) != filesz:
                raise ValueError('Vita plain segment size mismatch')
            item['sha256'] = sha(raw)
            (output / f'vita-segment-{i}.bin').write_bytes(raw)
            printable = [m[0].decode('ascii') for m in re.finditer(rb'[ -~]{6,}', raw)]
            (output / f'vita-segment-{i}-strings.txt').write_text('\n'.join(printable), encoding='utf-8')
        entries.append(item)
    return {'sha256': sha(data), 'elf_offset': elf, 'segments': entries}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', type=Path, default=Path('.'))
    ap.add_argument('--output', type=Path, default=Path('research/cross-platform'))
    ap.add_argument('--quickbms', type=Path)
    args = ap.parse_args()
    root = args.root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(root / 'tools/python'))
    try:
        from PIL import Image
    except ImportError:
        Image = None
    platforms, raw_assets, summary = {}, {}, {'date': '2026-10-07', 'packages': {}, 'xctd': [], 'comparisons': {}}
    sony_pvz = root / 'PS3 Image/PS3_GAME/USRDIR/NPUA-30059/USRDIR/data'
    sources = {'xbox': root / 'assets', 'ps3': sony_pvz,
               'vita': root / 'PSVita Image/USRDIR/data',
               'android': root / 'Chinese Android TV Image/assets/files'}
    for platform, source in sources.items():
        entries, packages = {}, []
        if platform == 'android':
            batches = [[(str(p.relative_to(source)).replace('\\', '/').lower(), p.read_bytes(),
                         {'container': str(p.relative_to(root)), 'offset': 0})
                        for p in sorted(source.rglob('*')) if p.is_file()]]
        else:
            batches = []
            for path in sorted(source.glob('*.pak')):
                original = path
                if path.open('rb').read(4) == bytes.fromhex('0ff512ed'):
                    path, decoded = decode_xctd(path, output / 'xbox-decoded', args.quickbms)
                    summary['xctd'].append(decoded)
                batch = pak_entries(path, platform != 'xbox')
                for _, _, meta in batch:
                    meta['package'] = original.name
                packages.append({'name': original.name, 'size': original.stat().st_size,
                                 'sha256': sha(original.read_bytes()), 'entries': len(batch)})
                batches.append(batch)
        raw_assets[platform] = {}
        for batch in batches:
            for name, raw, meta in batch:
                key = meta.get('package', '') + ':' + name
                record = {'name': name, **meta, 'size': len(raw), 'sha256': sha(raw), 'head': raw[:24].hex()}
                # Explicitly package-qualified keys retain localized collisions.
                entries[key] = record
                raw_assets[platform][key] = raw
                if Image and (raw[:4] == b'DDS ' or raw[:4] == b'\x89PNG' or raw[:3] == b'\xff\xd8\xff' or raw[:3] == b'GIF'):
                    try:
                        im = Image.open(io.BytesIO(raw)).convert('RGBA')
                        record['image'] = {'width': im.width, 'height': im.height,
                                           'rgba_sha256': sha(im.tobytes())}
                    except (OSError, ValueError) as e:
                        record['image_error'] = str(e)
                elif platform == 'xbox' and name.endswith('.ptx') and len(raw) >= 16:
                    w, h, stride, fmt = struct.unpack('>4I', raw[-16:])
                    record['ptx'] = {'width': w, 'height': h, 'stride': stride, 'format': hex(fmt)}
                    if Image:
                        try:
                            im = Image.open(io.BytesIO(xbox_dds(raw))).convert('RGBA')
                            record['image'] = {'width': im.width, 'height': im.height,
                                               'rgba_sha256': sha(im.tobytes())}
                        except (OSError, ValueError) as e:
                            record['image_error'] = str(e)
                # Expose readable evidence without extracting the full graphics library.
                if name.endswith(('.xml', '.jsx', '.jsfl', '.reanim', '.bat', '.menu.txt')) or 'lawnstrings' in name or name == 'properties/test.txt':
                    txt = decode_text(raw)
                    if txt is not None:
                        dest = output / 'text' / platform / meta.get('package', 'loose') / name
                        dest.parent.mkdir(parents=True, exist_ok=True)
                        dest.write_text(txt.replace('\r\n', '\n'), encoding='utf-8', newline='\n')
        platforms[platform] = entries
        save_json(output / f'{platform}-inventory.json', list(entries.values()))
        summary['packages'][platform] = {'packages': packages, 'entries': len(entries),
            'raw_bytes': sum(x['size'] for x in entries.values()),
            'extensions': dict(Counter(Path(x['name']).suffix for x in entries.values()))}
    # Compare common logical paths in the base+English resource view.
    views = {}
    for platform, entries in platforms.items():
        views[platform] = {x['name']: x for x in entries.values()
                           if x.get('package', '') in ('', 'main.pak', 'compressed.pak', 'sounds.pak', 'en.pak')}
    for a, b in itertools.combinations(views, 2):
        aa, bb = views[a], views[b]
        common = sorted(aa.keys() & bb.keys())
        exact = [n for n in common if aa[n]['sha256'] == bb[n]['sha256']]
        pixel = [n for n in common if aa[n].get('image') and bb[n].get('image') and
                 aa[n]['image'] == bb[n]['image']]
        info = {'common_paths': len(common), 'identical_bytes': len(exact), 'identical_images': len(pixel),
                'identical_names': exact, 'pixel_identical_names': pixel,
                'only_left': sorted(aa.keys() - bb.keys()), 'only_right': sorted(bb.keys() - aa.keys()),
                'different': [n for n in common if n not in exact]}
        save_json(output / f'compare-{a}-{b}.json', info)
        summary['comparisons'][a + '-' + b] = {k: info[k] for k in ('common_paths', 'identical_bytes', 'identical_images')}
    strings = {}
    for platform in platforms:
        strings[platform] = {}
        for key, record in platforms[platform].items():
            if record['name'].endswith('lawnstrings.txt') and record.get('package', '') in ('', 'en.pak', 'main.pak'):
                strings[platform] = lawn_strings(raw_assets[platform][key])
        save_json(output / f'{platform}-strings.json', strings[platform])
    for a, b in itertools.combinations(strings, 2):
        aa, bb = strings[a], strings[b]
        save_json(output / f'strings-{a}-{b}.json', {'left_count': len(aa), 'right_count': len(bb),
            'common': len(aa.keys() & bb.keys()), 'only_left': sorted(aa.keys() - bb.keys()),
            'only_right': sorted(bb.keys() - aa.keys()),
            'changed': {k: {a: aa[k], b: bb[k]} for k in sorted(aa.keys() & bb.keys()) if aa[k] != bb[k]}})
    summary['android_binary'] = symbol_analysis(root, output)
    summary['vita_binary'] = vita_segments(root, output)
    summary['string_counts'] = {p: len(s) for p, s in strings.items()}
    save_json(output / 'summary.json', summary)
    print(json.dumps({k: v for k, v in summary.items() if k not in ('android_binary', 'vita_binary')}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
