"""Inventory Xbox widget handlers from MSVC RTTI, with Android/Vita names.

No cross-platform address matching is inferred from mnemonic similarity.
Xbox primary vtables and Widget inheritance are read from this XEX's RTTI.
"""
from pathlib import Path
from collections import defaultdict
import json, re, struct, hashlib

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'research/input-coverage'
BASE = 0x82000000
data = (ROOT / '.cache/pvz-image.bin').read_bytes()
words = struct.unpack('>' + str(len(data)//4) + 'I', data)
refs = defaultdict(list)
for index, value in enumerate(words):
    if BASE <= value < BASE + len(data): refs[value].append(BASE + index * 4)
def read(address):
    if address < BASE or address + 4 > BASE + len(data): return 0
    return struct.unpack_from('>I', data, address - BASE)[0]
def name(address):
    offset = address + 8 - BASE
    if offset < 0 or offset >= len(data): return ''
    return data[offset:offset+200].split(b'\0')[0].decode('ascii', 'replace')
def class_name(rtti):
    parts = rtti.removeprefix('.?AV').removesuffix('@@').split('@')
    return '::'.join(reversed(parts))
types = {BASE + m.start() - 8: m.group().decode('ascii') for m in re.finditer(rb'\.\?AV[^\x00]{1,150}(?=\x00)', data)}
functions = sorted(set(int(a,16) for a in re.findall(r'sub_([0-9A-Fa-f]{8})', (ROOT/'generated/default/pvz_funcs.h').read_text())))
function_set = set(functions)
sizes = {a:b-a for a,b in zip(functions, functions[1:])}
source = '\n'.join(p.read_text(encoding='utf-8-sig') for p in (ROOT/'src').glob('*') if p.suffix in ('.cpp','.h'))
hooks = {int(a,16) for a in re.findall(r'REX_HOOK_RAW\(sub_([0-9A-Fa-f]{8})\)', source)}
android = json.loads((ROOT/'research/cross-platform/android-symbols.json').read_text())
handler_re = re.compile(r'::(Mouse\w+|GameButtonDown|GameButtonUp|GameAxisMove|OnButtonDown|OnButtonUp|KeyDown|KeyUp|ButtonDepress)\(')
android_handlers = [s for s in android if s.get('section') and s.get('type') == 2 and handler_re.search(s.get('demangled','')) and not s['demangled'].startswith('non-virtual')]
vita = json.loads((ROOT/'research/cross-platform/vita-touch/vtables.json').read_text())
slots = {'game_button_down':0xC0, 'game_button_up':0xC4,
    'mouse_move':0xD4, 'mouse_down_buttons':0xD8, 'mouse_down':0xDC,
    'mouse_up_buttons':0xE0, 'mouse_up':0xE4, 'mouse_up_point':0xE8,
    'mouse_drag':0xEC, 'mouse_wheel':0xF0}
def empty_method(address, depth=0):
    if depth > 4: return False
    first = read(address)
    if first == 0x4E800020: return True  # blr
    if first >> 26 == 18 and not first & 3:  # unconditional relative tail call
        displacement = first & 0x03FFFFFC
        if displacement & 0x02000000: displacement -= 0x04000000
        return empty_method(address + displacement, depth + 1)
    return False

coverage = json.loads((OUT/'routes.json').read_text(encoding='utf-8'))
rows = []
for type_at, rtti in types.items():
    for type_ref in refs[type_at]:
        col = type_ref - 12
        if read(col) != 0 or read(col+4) != 0 or read(col+8) > 0x1000: continue
        hierarchy = read(col+16)
        count, bases = read(hierarchy+8), read(hierarchy+12)
        if not 1 <= count <= 64: continue
        base_names = [name(read(read(bases+i*4))) for i in range(count)]
        widget = '.?AVWidget@Sexy@@' in base_names
        controls = 'Controls' in rtti and 'Touch' not in rtti
        if not widget and not controls: continue
        for col_ref in refs[col]:
            vt = col_ref + 4
            if read(vt) not in function_set: continue
            label = class_name(rtti)
            methods = {}
            selected_slots = slots if widget else {'button_down':0, 'button_up':4, 'update':12, 'draw':16}
            for method, offset in selected_slots.items():
                address = read(vt+offset)
                if address not in function_set: continue
                native_noop = empty_method(address)
                # Widget's three-argument methods only dispatch their leaf
                # overloads. An inherited wrapper is not evidence of input.
                if method == 'mouse_down' and address in (0x82473000, 0x8254C498):
                    native_noop = empty_method(read(vt+0xD8))
                if method == 'mouse_up' and address == 0x82473050:
                    native_noop = empty_method(read(vt+0xE0)) and empty_method(read(vt+0xE8))
                methods[method] = {'slot':hex(offset), 'address':f'0x{address:08X}',
                    'range_bytes':sizes.get(address), 'noop':native_noop, 'hooked':address in hooks}
            refs_android = [s['demangled'] for s in android_handlers if s['demangled'].startswith(label+'::')]
            row = {'class':label, 'vtable':f'0x{vt:08X}', 'rtti':f'0x{col:08X}',
                'kind':'widget' if widget else 'controls', 'methods':methods, 'android_handlers':refs_android,
                'vita_controls':[t['class'] for t in vita if t['class'].removesuffix('Touch') == label],
                'route':coverage.get(label, {'status':'unreviewed', 'note':'No route declaration; investigate before calling native handlers.'})}
            rows.append(row)
rows = sorted({(r['class'],r['vtable']):r for r in rows}.values(), key=lambda r:(r['class'],r['vtable']))
OUT.mkdir(parents=True, exist_ok=True)
result = {'method':'Xbox RTTI primary vtables and native overload dispatch. Hook presence is not proof of behavioral coverage. Android names are semantic references, not Xbox symbols.',
    'image_sha256':hashlib.sha256(data).hexdigest(),
    'widget_count':sum(r['kind']=='widget' for r in rows), 'controls_count':sum(r['kind']=='controls' for r in rows),
    'android_handler_count':len(android_handlers), 'rows':rows}
(OUT/'handlers.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
(OUT/'android-handlers.json').write_text(json.dumps(android_handlers,indent=2),encoding='utf-8')
owners = defaultdict(list)
for symbol in android_handlers:
    match = handler_re.search(symbol['demangled'])
    owners[symbol['demangled'][:match.start()]].append(symbol['demangled'])
native_names = {r['class'] for r in rows}
extra_routes = {
    'Challenge':'Native mode/hit dispatch: match puzzles, slot lever, aquarium food, hammer, vase hit 23. Includes unresolved special actions listed in docs/input-coverage.md.',
    'Coin':'Discrete native collection and dropped-seed capture; no held sun magnet.',
    'SeedPacket':'Native pickup/usability/cost/cooldown routes via bank geometry.',
    'SeedBank':'Shared slot/conveyor selection, wheel in console mode and PC drag gestures.',
    'ZenGarden':'Native tools and save/switch/shop through Xbox controls and Board hit testing.',
    'CutScene':'Native Board A for Dave/scripted clicks; manual check all tutorial branches.',
    'Plant':'Native planting/tool/hammer paths. Specialized Cob Cannon targeting remains a research gap.',
    'LawnApp':'Native screen/dialog transitions; host Escape routes gameplay pause.',
}
families = [{'class':owner,'count':len(names),'methods':names,
    'status':'xbox-widget-or-controls' if owner in native_names else 'object-dispatch' if owner in extra_routes else 'reference-only',
    'note':coverage[owner]['note'] if owner in coverage else extra_routes.get(owner,
        'No matching Xbox primary Widget/Controls RTTI. Android-only or unresolved class; not patched by copying an Android signature.')}
    for owner,names in sorted(owners.items())]
(OUT/'android-families.json').write_text(json.dumps(families,indent=2),encoding='utf-8')
header = ['// Generated by scripts/audit_input_handlers.py from Xbox RTTI.',
    '#pragma once', '#include <cstdint>', '#include <string_view>',
    'namespace pvz {', 'struct InputWidgetName { uint32_t vtable; std::string_view name; };',
    'inline constexpr InputWidgetName input_widget_names[] = {']
header += ['  {'+r['vtable']+'u, "'+r['class']+'"},' for r in rows]
header += ['};', 'inline std::string_view InputClassName(uint32_t vt) {',
    '  for (const auto& entry : input_widget_names) if (entry.vtable == vt) return entry.name;',
    '  return "unknown widget";', '}', '}']
header_path = ROOT/'src/input_widget_inventory.h'
header_text = '\n'.join(header)+'\n'
if not header_path.exists() or header_path.read_text() != header_text:
    header_path.write_text(header_text,encoding='utf-8')
lines = ['# Xbox input inventory', '', 'Generated by scripts/audit_input_handlers.py from the local Xbox image.',
    'Noop resolves inherited overload dispatch as well as direct empty methods. Route status is a reviewed declaration, not an automatic claim that a UI was tested.',
    '', '| Class | Vtable | Down | Up | Drag | Wheel | Route |', '|---|---|---|---|---|---|---|']
for row in rows:
    if row['kind'] != 'widget': continue
    cells = []
    for key in ('mouse_down','mouse_up','mouse_drag','mouse_wheel'):
        m = row['methods'].get(key)
        cells.append('—' if not m else m['address'] + (' noop' if m['noop'] else '') + (' hook' if m['hooked'] else ''))
    lines.append('| '+row['class']+' | '+row['vtable']+' | '+' | '.join(cells)+' | '+row['route']['status']+' |')
(OUT/'handlers.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
docs_inventory = ROOT/'docs/input-inventory'
docs_inventory.mkdir(parents=True, exist_ok=True)
for leaf in ('handlers.md', 'handlers.json', 'android-families.json'):
    (docs_inventory/leaf).write_bytes((OUT/leaf).read_bytes())
print(json.dumps({k:v for k,v in result.items() if k != 'rows'},indent=2))
print('Unreviewed classes:', ', '.join(r['class'] for r in rows if r['route']['status']=='unreviewed') or 'none')
