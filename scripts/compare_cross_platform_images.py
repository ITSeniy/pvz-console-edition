"""Decode research previews and recover Vita atlas rectangles from plain SELF data.

Run after analyze_cross_platform.py. No supplied images or runtime assets change.
Vita's 44-byte atlas records are inferred from this image and validated by bounds;
GXT support is deliberately limited to its swizzled BC3 atlases.
"""
import io
import json
from pathlib import Path
import struct
import sys

from analyze_cross_platform import pak_entries, xbox_dds, save_json

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/python'))
from PIL import Image, ImageDraw, ImageFont, ImageChops, ImageStat


def gxt_image(raw, x_even=False):
    if raw[:4] != b'GXT\0':
        raise ValueError('Not GXT')
    count, dataoff, datasize = struct.unpack_from('<3I', raw, 8)
    offset, size, palette, flags, kind, fmt = struct.unpack_from('<6I', raw, 32)
    w, h = struct.unpack_from('<HH', raw, 56)
    if count != 1 or kind != 0 or fmt != 0x87000000 or w & (w - 1) or h & (h - 1):
        raise ValueError(f'Unsupported GXT: {count} {kind:x} {fmt:x} {w}x{h}')
    bw, bh = w // 4, h // 4
    bits = min(bw, bh).bit_length() - 1
    src = raw[offset:offset + size]
    if len(src) != bw * bh * 16:
        raise ValueError('Wrong GXT BC3 data size')
    out = bytearray(len(src))
    for y in range(bh):
        for x in range(bw):
            index = 0
            for b in range(bits):
                index |= ((x >> b) & 1) << (2 * b + (0 if x_even else 1))
                index |= ((y >> b) & 1) << (2 * b + (1 if x_even else 0))
            index |= ((x >> bits) if bw >= bh else (y >> bits)) << (2 * bits)
            dest = (y * bw + x) * 16
            out[dest:dest + 16] = src[index * 16:index * 16 + 16]
    # xbox_dds provides a checked DXT5 DDS header; data here is already LE.
    header = xbox_dds(b'\0' * ((bw * 16) * bh) + struct.pack('>4I', w, h, bw * 16, 0x5400201a))[:128]
    return Image.open(io.BytesIO(header + out)).convert('RGBA')


def vita_atlas_records():
    text = (ROOT / 'research/cross-platform/vita-segment-0.bin').read_bytes()
    data = (ROOT / 'research/cross-platform/vita-segment-1.bin').read_bytes()
    def string(ptr):
        off = ptr - 0x81000000
        if not 0 <= off < len(text):
            return None
        end = text.find(b'\0', off, off + 180)
        if end < 0:
            return None
        try:
            return text[off:end].decode('ascii')
        except UnicodeDecodeError:
            return None
    entries = []
    for off in range(0, len(data) - 44, 4):
        n, a = struct.unpack_from('<II', data, off)
        name, atlas = string(n), string(a)
        if not name or not atlas or not name.startswith(('images/', 'reanim/', 'data/')) or not atlas.endswith('.ptx'):
            continue
        x, y, w, h, original_w, original_h, rotated = struct.unpack_from('<6HI', data, off + 8)
        if not w or not h or w > 4096 or h > 4096 or rotated > 1:
            continue
        entries.append({'record_address': hex(0x8144b000 + off), 'name': name.lower(),
                        'atlas': atlas.lower(), 'x': x, 'y': y, 'width': w, 'height': h,
                        'original_width': original_w, 'original_height': original_h, 'rotated': rotated})
    return entries


def main():
    output = ROOT / 'research/cross-platform/previews'
    output.mkdir(exist_ok=True)
    banks = {}
    for platform, paths, sony in [
        ('xbox', [ROOT / 'research/cross-platform/xbox-decoded/main.pak', ROOT / 'assets/en.pak'], False),
        ('ps3', list((ROOT / 'PS3 Image/PS3_GAME/USRDIR/NPUA-30059/USRDIR/data').glob('*.pak')), True),
        ('vita', [ROOT / 'PSVita Image/USRDIR/data/main.pak', ROOT / 'PSVita Image/USRDIR/data/en.pak'], True)]:
        banks[platform] = {name: raw for path in paths for name, raw, meta in pak_entries(path, sony)}
    records = vita_atlas_records()
    save_json(output.parent / 'vita-atlas-records.json', records)
    cached = {}
    wanted = ['images/background1', 'images/seedbank', 'images/seedchooser_background',
              'images/shovel', 'images/lawnmoweroverlay', 'images/help_buttons', 'images/challenge_thumbnails',
              'images/flagmeter']
    image_records = []
    canvas = Image.new('RGB', (1280, 120 + len(wanted) * 225), '#edf0f3')
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 18)
    small = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 14)
    draw.text((18, 12), 'PvZ asset comparison - supplied images, 2026-10-07', font=font, fill='#17202b')
    for i, p in enumerate(['xbox', 'ps3', 'vita', 'android']):
        draw.text((i * 320 + 18, 48), p.upper(), font=font, fill='#17202b')
    for row, name in enumerate(wanted):
        imgs = {}
        evidence = {'name': name, 'platforms': {}}
        for platform in ('xbox', 'ps3'):
            raw = banks[platform].get(name + '.ptx')
            if raw:
                imgs[platform] = Image.open(io.BytesIO(xbox_dds(raw) if platform == 'xbox' else raw)).convert('RGBA')
        candidates = [r for r in records if r['name'] == name and r['atlas'] in banks['vita']]
        if candidates:
            rec = candidates[0]
            raw = banks['vita'][rec['atlas']]
            try:
                key = rec['atlas']
                if key not in cached:
                    cached[key] = gxt_image(raw)
                atlas = cached[key]
                x, y, w, h = [rec[k] for k in ['x', 'y', 'width', 'height']]
                physical_w, physical_h = (h, w) if rec['rotated'] else (w, h)
                if x + physical_w > atlas.width or y + physical_h > atlas.height:
                    raise ValueError('Atlas rectangle out of bounds')
                cropped = atlas.crop((x, y, x + physical_w, y + physical_h))
                if rec['rotated']:
                    cropped = cropped.transpose(Image.Transpose.ROTATE_90)
                imgs['vita'] = cropped
                evidence['vita_atlas'] = rec
            except ValueError as e:
                evidence['vita_decode_error'] = str(e)
        android = ROOT / 'Chinese Android TV Image/assets/files'
        # Paths have mixed capitalization on disk; Windows resolves case.
        for ext in ['.png', '.jpg', '.gif']:
            path = android / (name + ext)
            if path.exists():
                im = Image.open(path).convert('RGBA')
                alpha = android / (name + '_.gif')
                if alpha.exists():
                    mask = Image.open(alpha).convert('L')
                    if mask.size == im.size:
                        im.putalpha(mask)
                imgs['android'] = im
                break
        if 'xbox' in imgs and 'ps3' in imgs and imgs['xbox'].size == imgs['ps3'].size:
            diff = ImageChops.difference(imgs['xbox'], imgs['ps3'])
            evidence['xbox_ps3_mean_absolute_rgba_error'] = ImageStat.Stat(diff).mean
        for col, platform in enumerate(['xbox', 'ps3', 'vita', 'android']):
            x, y = col * 320 + 14, 96 + row * 225
            draw.rectangle((x, y, x + 292, y + 160), fill='#d2d8df')
            im = imgs.get(platform)
            draw.text((x, y + 165), name.removeprefix('images/'), font=small, fill='#17202b')
            if im:
                evidence['platforms'][platform] = {'width': im.width, 'height': im.height}
                im.save(output / f'{row}-{platform}.png')
                thumb = im.copy()
                thumb.thumbnail((288, 156))
                canvas.paste(thumb, (x + (292 - thumb.width) // 2, y + (160 - thumb.height) // 2), thumb)
                draw.text((x, y + 187), f'{im.width} x {im.height}', font=small, fill='#394c63')
            else:
                draw.text((x + 20, y + 60), 'No corresponding decoded image', font=small, fill='#526279')
        image_records.append(evidence)
    canvas.save(output / 'comparison.png')
    save_json(output.parent / 'image-comparison.json', image_records)
    print(json.dumps({'atlas_records': len(records), 'previews': str(output / 'comparison.png')}, indent=2))


if __name__ == '__main__':
    main()
