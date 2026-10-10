"""Extract PS3 controller textures and convert DDS DXT5 to Xbox PTX.

Usage: python scripts/extract_ps3_controller_assets.py path/to/main.pak
Only the named controller resources are emitted; no package is modified.
"""
import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path

NAMES = {
    'images\\help_buttons.ptx': 'help_buttons',
    'images\\help_buttons_small.ptx': 'help_buttons_small',
    'images\\gamepad0.ptx': 'gamepad0',
    'images\\gamepad1.ptx': 'gamepad1',
    'images\\help_menu_image_controller.ptx': 'help_menu_image_controller',
    'images\\help_menu_image_vs_controllers.ptx': 'help_menu_image_vs_controllers',
    'reanim\\a_button.ptx': 'a_button',
}

def xbox_ptx(dds):
    if dds[:4] != b'DDS ' or dds[84:88] != b'DXT5':
        raise ValueError('Expected a PS3 DDS DXT5 texture')
    height, width = struct.unpack_from('<II', dds, 12)
    stride = ((width + 127) // 128) * 512
    row_size = ((width + 3) // 4) * 16
    rows = (height + 3) // 4
    pixels = dds[128:128 + rows * row_size]
    if len(pixels) != rows * row_size:
        raise ValueError('Truncated DDS')
    result = bytearray()
    for row in range(rows):
        blocks = pixels[row * row_size:(row + 1) * row_size]
        swapped = bytearray(len(blocks))
        swapped[::2], swapped[1::2] = blocks[1::2], blocks[::2]
        result += swapped + b'\xcd' * (stride - row_size)
    result += b'\xcd' * stride
    result += struct.pack('>IIII', width, height, stride, 0x5400201A)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pak', type=Path)
    parser.add_argument('--output', type=Path, default=Path('assets/pc/dualshock'))
    args = parser.parse_args()
    data = args.pak.read_bytes()
    if data[:8] != bytes.fromhex('c04ac0ba00000000'):
        raise ValueError('Expected the PS3 packed-file header')
    pos, entries = 8, []
    while data[pos] != 0x80:
        flags, size = data[pos:pos + 2]
        if flags != 0: raise ValueError('Unsupported entry flags')
        pos += 2
        name = data[pos:pos + size].decode('ascii').lower()
        pos += size
        compressed, expanded = struct.unpack_from('<II', data, pos)
        pos += 16
        entries.append((name, compressed, expanded))
    pos += 1
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for name, compressed, expanded in entries:
        padding = struct.unpack_from('<H', data, pos)[0]
        pos += 2 + padding
        payload = data[pos:pos + compressed]
        pos += compressed
        if name not in NAMES: continue
        raw = zlib.decompress(payload) if expanded else payload
        if expanded and len(raw) != expanded: raise ValueError('Decompression size mismatch')
        converted = xbox_ptx(raw)
        output = args.output / (NAMES[name] + '.ptx')
        output.write_bytes(converted)
        width, height, _, _ = struct.unpack('>IIII', converted[-16:])
        manifest.append({'source': name, 'file': output.name, 'width': width, 'height': height,
                         'source_sha256': hashlib.sha256(raw).hexdigest(),
                         'sha256': hashlib.sha256(converted).hexdigest()})
        if NAMES[name] in ('help_buttons', 'help_buttons_small'):
            # Transparent DXT5 blocks: the all-zero alpha endpoints decode to
            # zero alpha. Keep the same row stride, padding and PTX footer.
            empty = b'\0' * (len(converted) - 16) + converted[-16:]
            (args.output / (NAMES[name] + '_empty.ptx')).write_bytes(empty)
    if len(manifest) != len(NAMES): raise ValueError('Some controller resources are missing')
    (args.output / 'sources.json').write_text(json.dumps({'source_package_sha256':
        hashlib.sha256(data).hexdigest(), 'assets': manifest}, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__': main()

