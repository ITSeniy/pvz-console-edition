"""Bounded, read-only Vita Touch disassembly and comparison evidence.

Requires scripts/analyze_cross_platform.py outputs and local tools/python Capstone.
No executable is run or modified. Function boundaries come from module .ARM.exidx.
"""
from pathlib import Path
from collections import defaultdict
import json
import re
import struct
import sys

from analyze_cross_platform import save_json, sha

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'research/cross-platform'
OUT = SOURCE / 'vita-touch'
sys.path.insert(0, str(ROOT / 'tools/python'))
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB

# Semantic aliases established by inspecting this image; these are not recovered
# original symbol names. Vita's control vtables differ from Android's layout.
ALIASES = {
    0x8104b4c8: 'App.front_touch_to_widget_events',
    0x8104bc3c: 'App.map_front_touch_coordinates',
    0x810d52ac: 'Touch.front_callback', 0x810d5446: 'Touch.rear_callback',
    0x810d54d6: 'Touch.register_primary_contacts',
    0x810d8b20: 'Touch.init_tracker', 0x810d8bd8: 'Touch.track_contact_ids',
    0x810d8f62: 'Touch.dispatch_callbacks', 0x810d903a: 'Touch.normalize_and_dispatch',
    0x810d92c2: 'Touch.init_panels', 0x810d94f6: 'Touch.poll_both_panels',
    0x810d95b8: 'Touch.register_callback',
    0x810f777e: 'Board.Draw.rear_press_branch',
    0x810f0b0e: 'Board.MouseMove', 0x810f0b40: 'Board.MouseDrag',
    0x810f0b72: 'Board.MouseDown', 0x810f0b94: 'Board.MouseUp',
    0x810fcd4c: 'Board.GameButtonDown', 0x810fcf7e: 'Board.GameButtonUp',
    0x810fd2cc: 'Board.touch_down_button_20',
    0x810fd2de: 'Board.copy_touch_coordinates',
    0x810fd304: 'Board.touch_up_button_20_then_clear_coordinates',
    0x811bf960: 'Motion.coin_collection',
    0x811c1512: 'Touch.select_seed_packet',
    0x811c179a: 'GamepadControlsTouch.OnButtonDown',
    0x811c2ada: 'GamepadControlsTouch.OnButtonUp',
    0x811c343c: 'GamepadControlsTouch.Update',
    0x811c55bc: 'GamepadControlsTouch.Draw',
    0x811c6148: 'GamepadControlsTouch.PickupUsableSeedPacket',
    0x811c6350: 'GamepadControlsTouch.is_touch_on_lawn',
    0x811c689c: 'TreeOfWisdomControlsTouch.OnButtonUp',
    0x811c7360: 'ZenGardenControlsTouch.OnButtonUp',
    0x8118a538: 'SeedPacket.CanPickUp',
    0x8118b142: 'SeedPacket.activate_selected_packet',
    0x810f064c: 'Board.CanPlantAt', 0x810f9468: 'Board.MouseHitTest',
    0x810f9c94: 'Board.MouseDownWithPlant', 0x810facd0: 'Board.MouseDownWithTool',
    0x810eb998: 'Board.ClearCursor', 0x810ef144: 'Board.refresh_seed_from_cursor',
}
KNOWN_NIDS = {
    0x3e4f4a81: {0x10a2ca25: 'sceTouchGetPanelInfo',
                 0x1b9c5d14: 'sceTouchSetSamplingState', 0xff082df0: 'sceTouchPeek'},
    0xdc571b3f: {0x28034ac9: 'sceMotionStartSampling', 0xbdb32767: 'sceMotionGetState'},
}


class Image:
    def __init__(self):
        self.segments = [(0x81000000, (SOURCE / 'vita-segment-0.bin').read_bytes()),
                         (0x8144b000, (SOURCE / 'vita-segment-1.bin').read_bytes())]
        self.cs = {True: Cs(CS_ARCH_ARM, CS_MODE_THUMB), False: Cs(CS_ARCH_ARM, CS_MODE_ARM)}
        self.functions = {}
        self.names = defaultdict(list)
        original = (ROOT / 'PSVita Image/eboot.bin').read_bytes()
        elf = struct.unpack_from('<Q', original, 0x40)[0]
        module = struct.unpack_from('<I', original, elf + 24)[0]
        raw = self.segments[0][1]
        assert raw[module + 4:module + 10] == b'PvZPSP'
        top, end = struct.unpack_from('<2I', raw, module + 0x4c)
        assert top < end <= len(raw) and (end - top) % 8 == 0
        entries = []
        for off in range(top, end, 8):
            prel = struct.unpack_from('<I', raw, off)[0] & 0x7fffffff
            if prel & 0x40000000:
                prel -= 0x80000000
            entries.append(0x81000000 + off + prel)
        assert all((a & ~1) < (b & ~1) for a, b in zip(entries, entries[1:]))
        assert entries[-1] == 0x81000000 + module
        for ptr, following in zip(entries, entries[1:]):
            address = ptr & ~1
            self.functions[address] = {'address': address, 'pointer': ptr,
                                       'size': (following & ~1) - address, 'thumb': bool(ptr & 1)}
        self.starts = sorted(self.functions)
        self.metadata = {'eboot_sha256': sha(original), 'segment_sha256': [sha(s) for _, s in self.segments],
                         'module_info': hex(0x81000000 + module), 'exidx_start': hex(0x81000000 + top),
                         'exidx_end': hex(0x81000000 + end), 'bounded_functions': len(self.functions)}
        assert sha(original) == '1dd200eb9ad31826ace7844f11a5aead003bf57d83aaaccb3a7bc3ab49414afd'
        assert self.metadata['segment_sha256'] == [
            '8f40ffddeb690918dbbd81fa2ccabfff69ef550d9cf92a5049a125d65aa0bbab',
            '7a4fd1339fb6b09e9c65720339eb131c52cd977790b55a586b9b518bce4c3bcf']
        self.module = module
        for addr, alias in ALIASES.items():
            assert addr in self.functions, f'Missing code boundary for {alias}'
            self.names[addr].append('alias:' + alias)

    def read(self, addr, size):
        for base, raw in self.segments:
            if base <= addr and addr + size <= base + len(raw):
                return raw[addr - base:addr - base + size]
        raise ValueError(f'Unmapped address {addr:#x}, size {size:#x}')

    def instructions(self, addr):
        f = self.functions[addr]
        return list(self.cs[f['thumb']].disasm(self.read(addr, f['size']), addr))

    def imports(self):
        raw = self.segments[0][1]
        top, end = struct.unpack_from('<2I', raw, self.module + 0x2c)
        assert top < end <= len(raw)
        result = []
        off = top
        while off < end:
            size = raw[off]
            assert size == 0x34, f'Unsupported import record size {size:#x}'
            count = struct.unpack_from('<H', raw, off + 6)[0]
            nid, name = struct.unpack_from('<2I', raw, off + 0x10)
            nids, entries = struct.unpack_from('<2I', raw, off + 0x1c)
            functions = []
            for i in range(count):
                fnid = struct.unpack('<I', self.read(nids + i * 4, 4))[0]
                ptr = struct.unpack('<I', self.read(entries + i * 4, 4))[0]
                label = KNOWN_NIDS.get(nid, {}).get(fnid)
                if label:
                    self.names[ptr & ~1].append('import:' + label)
                functions.append({'nid': hex(fnid), 'pointer': hex(ptr),
                                  'address': hex(ptr & ~1), 'name': label})
            result.append({'address': hex(0x81000000 + off), 'library_nid': hex(nid),
                           'name': self.string(name), 'functions': functions})
            off += size
        assert off == end
        touch = next(t for t in result if t['library_nid'] == '0x3e4f4a81')
        assert {f['name']: f['address'] for f in touch['functions']} == {
            'sceTouchGetPanelInfo': '0x813aa620', 'sceTouchSetSamplingState': '0x813aa630',
            'sceTouchPeek': '0x813aa640'}
        return result

    def tables(self):
        names = ['BaseGamepadControls', 'GamepadControls', 'TreeOfWisdomControls', 'ZenGardenControls',
                 'BaseGamepadControlsTouch', 'GamepadControlsTouch', 'TreeOfWisdomControlsTouch',
                 'ZenGardenControlsTouch', 'HelpTouchDialog', 'HelpOptionsDialog', 'StoreScreenTouch',
                 'StoreScreen', 'Board', 'SeedChooserScreen', 'ChallengeScreen', 'Widget', 'WidgetManager']
        found = []
        for name in names:
            needle = str(len(name)).encode() + name.encode() + b'\0'
            for name_base, raw in self.segments:
                for match in re.finditer(re.escape(needle), raw):
                    name_addr = name_base + match.start()
                    for base, seg in self.segments:
                        for ref in re.finditer(re.escape(struct.pack('<I', name_addr)), seg):
                            if ref.start() % 4 or ref.start() < 4:
                                continue
                            rtti = base + ref.start() - 4
                            if not 0x81000000 <= struct.unpack_from('<I', seg, ref.start() - 4)[0] < 0x81584820:
                                continue
                            for vb, vs in self.segments:
                                for vr in re.finditer(re.escape(struct.pack('<I', rtti)), vs):
                                    off = vr.start()
                                    if off % 4 or off < 4 or struct.unpack_from('<i', vs, off - 4)[0] != 0:
                                        continue
                                    slots = []
                                    for idx in range(150):
                                        if off + 8 + idx * 4 > len(vs):
                                            break
                                        ptr = struct.unpack_from('<I', vs, off + 4 + idx * 4)[0]
                                        if (ptr & ~1) not in self.functions:
                                            break
                                        slots.append({'slot': idx, 'pointer': hex(ptr), 'address': hex(ptr & ~1),
                                                      'size': self.functions[ptr & ~1]['size']})
                                        self.names[ptr & ~1].append(f'{name}[{idx}]')
                                    if slots:
                                        found.append({'class': name, 'address': hex(vb + off + 4), 'slots': slots})
        return found

    def string(self, addr):
        try:
            raw = self.read(addr, 160).split(b'\0')[0]
            return raw.decode('ascii') if len(raw) >= 4 and all(32 <= b < 127 for b in raw) else ''
        except (ValueError, UnicodeDecodeError):
            return ''

    def dump(self, addr):
        f = self.functions[addr]
        lines = [f"\n{addr:08x} size={f['size']} thumb={f['thumb']} {' / '.join(self.names[addr])}"]
        # Only annotate adjacent MOVW/MOVT pairs; these are evidence, not full constant propagation.
        low = {}
        for ins in self.instructions(addr):
            annotation = ''
            if ins.mnemonic in ('bl', 'blx', 'b', 'b.w') and ins.op_str.startswith('#0x'):
                target = int(ins.op_str[1:], 16) & ~1
                annotation = ' / '.join(self.names[target])
            if ins.mnemonic == 'movw':
                reg, imm = ins.op_str.split(', ')
                low[reg] = (int(imm[1:], 0), ins.address)
            elif ins.mnemonic == 'movt':
                reg, imm = ins.op_str.split(', ')
                if reg in low and ins.address - low[reg][1] <= 16:
                    value = (int(imm[1:], 0) << 16) | low[reg][0]
                    s = self.string(value)
                    annotation += f' => {value:#x}' + (f' {s!r}' if s else '')
            lines.append(f'{ins.address:08x}  {ins.mnemonic:10s} {ins.op_str:38s} {annotation}')
        return lines


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    image = Image()
    tables = image.tables()
    imports = image.imports()
    touch_table = next(t for t in tables if t['class'] == 'GamepadControlsTouch')
    assert touch_table['address'] == '0x814491b8'
    assert [touch_table['slots'][i]['address'] for i in (0, 1, 3, 5, 9, 18)] == [
        '0x811c179a', '0x811c2ada', '0x811c343c', '0x811c55bc', '0x811c6350', '0x811c6148']
    image.metadata['limitations'] = [
        'EXIDX supplies bounded code ranges; this is not a complete symbol table.',
        'Linear disassembly may decode literal pools or stop at data.',
        'Direct edges are candidates only; indirect virtual calls require manual inspection.',
        'Aliases are semantic interpretations, not original Vita symbol names.',
        'No game executable is run.']
    save_json(OUT / 'metadata.json', image.metadata)
    save_json(OUT / 'vtables.json', tables)
    save_json(OUT / 'imports.json', imports)
    # Include all functions between BaseTouch and the last derived control implementation.
    selected = {a for a in image.functions if 0x811bfc00 <= a < 0x811c9000}
    selected.update(ALIASES)
    selected.update(int(f['address'], 16) for t in imports for f in t['functions'] if f['name'])
    selected.update(int(s['address'], 16) for t in tables for s in t['slots'])
    edges = []
    coverage = []
    for addr in image.functions:
        instructions = image.instructions(addr)
        decoded = sum(ins.size for ins in instructions)
        coverage.append({'address': hex(addr), 'range_bytes': image.functions[addr]['size'],
                         'decoded_bytes': decoded})
        for ins in instructions:
            if ins.mnemonic in ('bl', 'blx', 'b', 'b.w') and ins.op_str.startswith('#0x'):
                target = int(ins.op_str[1:], 16) & ~1
                if target in image.functions and target != addr:
                    edges.append({'from': hex(addr), 'at': hex(ins.address), 'to': hex(target), 'kind': ins.mnemonic})
    save_json(OUT / 'direct-edges.json', edges)
    save_json(OUT / 'decode-coverage.json', coverage)
    # Assertions tied to actual instructions, so a mislabeled image cannot
    # silently produce a plausible report from hardcoded semantic aliases.
    verified_calls = [(0x810d94f6, 0x813aa640), (0x810d92c2, 0x813aa620),
                      (0x810d92c2, 0x813aa630), (0x811c2ada, 0x8118a538),
                      (0x811c2ada, 0x810f064c), (0x811c2ada, 0x810f9c94),
                      (0x811c2ada, 0x810facd0), (0x811c1512, 0x8118a538)]
    edge_pairs = {(int(e['from'], 16), int(e['to'], 16)) for e in edges}
    assert all(pair in edge_pairs for pair in verified_calls)
    for addr in (0x810fd2cc, 0x810fd304):
        assert any(i.mnemonic == 'movs' and i.op_str == 'r1, #0x14'
                   for i in image.instructions(addr))
    save_json(OUT / 'verified-evidence.json', {
        'direct_calls': [{'from': hex(a), 'to': hex(b)} for a, b in verified_calls],
        'synthetic_button': 20, 'synthetic_dispatchers': ['0x810fd2cc', '0x810fd304'],
        'alias_policy': 'semantic, not original symbols',
        'semantic_aliases': {hex(a): name for a, name in ALIASES.items()}})
    children = {int(e['to'], 16) for e in edges if int(e['from'], 16) in selected}
    # One hop gives hit-test/state helpers while bounding output size.
    selected.update(children)
    (OUT / 'functions.txt').write_text('\n'.join(line for a in sorted(selected) for line in image.dump(a)) + '\n', encoding='utf-8')
    save_json(OUT / 'function-index.json', [{**f, 'address': hex(a), 'pointer': hex(f['pointer']),
               'labels': image.names[a]} for a, f in image.functions.items()])
    print(json.dumps({**image.metadata, 'tables': len(tables), 'dumped_functions': len(selected), 'direct_edges': len(edges)}))


if __name__ == '__main__':
    main()
