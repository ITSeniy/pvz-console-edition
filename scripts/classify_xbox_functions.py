"""T11b: classify Xbox functions and plan replacement batches (read-only analysis).

Inputs: generated/default/pvz_funcs.h, pvz_pch.h, .cache/pvz-image.bin,
accepted xbox-names.csv, hook-inventory.csv, Android symbol names, PC .cpp/.h,
and local documentation used to explain the plan. Outputs: compact T11 CSVs,
a deterministic Markdown report, full evidence and library zones under ignored
research/function-classes/. JSON/report include complete SHA-256 manifests.

Class evidence comes from Xbox MSVC RTTI/vtables, accepted T01 names, and
conservative PPC r3 identity/vtable-store/field/signature analysis. Android names never create
Xbox class assignments. No address-neighbour propagation or slot-name guessing.
Bytes mean disjoint address spans to the next generated entry (last: code end),
including padding/data; reachable_bytes is a separate conservative code metric.
Only direct reachable calls/tail branches enter the reference graph. Unknown
indirect dispatch stops traversal. Scheduling uses leaf fraction, port hooks,
PC availability and risk. Unknown functions stay in a separate unassigned list.

Run: python -I scripts/classify_xbox_functions.py [--check] [--self-test]
--check verifies existing files without writing; --sample prints fixed review
samples; --inspect HEX prints PPC instructions with Capstone from tools/python.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
import csv
import hashlib
import io
import json
from pathlib import Path
import random
import re
import struct
import sys

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]
BASE = 0x82000000
DATA = ROOT / 'docs/native-port/data'
SEED = 2026101802
RESEARCH = ROOT / 'research/function-classes'
REGIONS = [('game', 0x82120000, 0x822B0000),
           ('xbox-platform', 0x822B0000, 0x82300000),
           ('middleware', 0x82300000, 0x82470000),
           ('sexy', 0x82470000, 0x825B0000),
           ('crt-xdk', 0x825B0000, 0x100000000)]


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def packed(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':'))


def hx(value):
    return f'0x{value:08X}'


def region(address):
    return next((n for n, a, b in REGIONS if a <= address < b), 'outside')


def split_name(name):
    head = name.split('(')[0]
    depth, cut = 0, -1
    for i, char in enumerate(head):
        depth += (char == '<') - (char == '>')
        if depth == 0 and head.startswith('::', i):
            cut = i
    return (head[:cut], name[cut + 2:]) if cut >= 0 else ('', name)


class Inputs:
    def __init__(self):
        self.hashes = {}

    def read(self, path):
        path = ROOT / path
        raw = path.read_bytes()
        self.hashes[path.relative_to(ROOT).as_posix()] = digest(raw)
        return raw

    def text(self, path):
        return self.read(path).decode('utf-8-sig')

    def csv(self, path):
        return list(csv.DictReader(io.StringIO(self.text(path))))


class Image:
    def __init__(self, inputs):
        self.raw = inputs.read('.cache/pvz-image.bin')
        self.words = struct.unpack(f'>{len(self.raw)//4}I', self.raw)
        starts = re.findall(r'DECLARE_REX_FUNC\(sub_([0-9A-Fa-f]{8})\)',
                            inputs.text('generated/default/pvz_funcs.h'))
        self.funcs = sorted(int(a, 16) for a in starts)
        assert len(self.funcs) == len(set(self.funcs)), 'Duplicate declarations'
        self.fset = set(self.funcs)
        pch = inputs.text('generated/default/pvz_pch.h')
        code_base = int(re.search(r'REX_CODE_BASE\s+(0x[0-9A-Fa-f]+)', pch)[1], 16)
        code_size = int(re.search(r'REX_CODE_SIZE\s+(0x[0-9A-Fa-f]+)', pch)[1], 16)
        self.end = code_base + code_size
        self.ends = dict(zip(self.funcs, self.funcs[1:] + [self.end]))
        assert self.funcs[0] == code_base and self.funcs[-1] < self.end
        self.refs = defaultdict(list)
        for i, value in enumerate(self.words):
            if BASE <= value < BASE + len(self.raw):
                self.refs[value].append(BASE + 4*i)
        self.types = {}
        for m in re.finditer(rb'\.\?AV[^\x00]{1,150}(?=\x00)', self.raw):
            raw = m[0].decode('ascii', errors='replace')
            name = ('msvc:' + raw if '?$' in raw else
                    '::'.join(reversed(raw[4:-2].split('@'))))
            self.types[BASE + m.start() - 8] = name
        self.tables, self.bases = {}, {}
        for td, name in sorted(self.types.items()):
            for ref in self.refs[td]:
                col = ref - 12
                if self.rd(col) or self.rd(col+8) > 0x1000 or self.rd(col+4) > 0x10000:
                    continue
                hier = self.rd(col+16)
                count = self.rd(hier+8)
                if not 1 <= count <= 64:
                    continue
                arr = self.rd(hier+12)
                bases = [self.types.get(self.rd(self.rd(arr+4*i))) for i in range(count)]
                if not bases or bases[0] != name or None in bases:
                    continue
                self.bases[name] = set(bases[1:]) - {name}
                for vref in self.refs[col]:
                    slots, a = [], vref+4
                    while self.rd(a) in self.fset:
                        slots.append(self.rd(a))
                        a += 4
                    if slots:
                        self.tables[vref+4] = (name, self.rd(col+4), slots, td, col)

    def rd(self, address):
        return self.words[(address-BASE)//4] if BASE <= address <= BASE+len(self.raw)-4 and address % 4 == 0 else 0

    def string(self, address):
        offset = address-BASE
        if not 0 < offset < len(self.raw) or self.raw[offset-1] != 0:
            return None
        raw = self.raw[offset:offset+256].split(b'\0')[0]
        if len(raw) < 5 or not all(32 <= c < 127 or c in (9, 10, 13) for c in raw):
            return None
        return raw.decode('ascii') if re.search(rb'[A-Za-z]', raw) else None

    def owner(self, names):
        """Least-derived *observed* vtable host; reject unrelated ICF sharing."""
        roots = sorted(c for c in names if not (self.bases.get(c, set()) & names))
        return roots[0] if len(roots) == 1 else None


def signed(value, bits):
    return value - (1 << bits) if value & (1 << (bits-1)) else value


def branch(p, w):
    delta = signed(w & (0x3FFFFFC if w >> 26 == 18 else 0xFFFC),
                   26 if w >> 26 == 18 else 16)
    return (delta if w & 2 else p+delta) & 0xFFFFFFFF


def transfer(w, old):
    """Small PPC abstract interpreter: constants and exact entry-r3 identity.

    Unknown X-form writes kill both possible destinations; unsupported primary
    opcodes kill all state. Calls preserve only nonvolatile registers r14..r31.
    No stack reload or object-field inference, no assumed callee return types.
    """
    state = dict(old)
    op, rt, ra, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
    rb = (w >> 11) & 31
    def put(reg, value):
        state.pop(reg, None)
        if value is not None:
            state[reg] = value
    if op in (14, 15):
        value = 0 if ra == 0 else old.get(ra)
        delta = signed(imm, 16) * (65536 if op == 15 else 1)
        if isinstance(value, int):
            value = (value+delta) & 0xFFFFFFFF
        elif value == 'this' and delta == 0:
            pass
        else:
            value = None
        put(rt, value)
    elif op in (24, 25):
        value = old.get(rt)
        put(ra, value | (imm << (16 if op == 25 else 0)) if isinstance(value, int) else None)
    elif op == 31:
        xo = (w >> 1) & 1023
        if xo == 444 and rt == rb:  # or rA,rS,rS / mr
            put(ra, old.get(rt))
        elif xo in (0, 32, 144, 467, 150, 151, 183, 215, 247, 407, 439, 598, 854):
            # Compare, mtcrf/mtspr, stores, sync/eieio. Indexed update stores
            # kill their base; stwcx. does not update a GPR.
            if xo in (183, 247, 439):
                put(ra, None)
        else:
            put(rt, None)
            put(ra, None)
    elif op in (7, 8, 12, 13, 32, 33, 34, 35, 40, 41, 42, 43, 58):
        put(rt, None)
        if op in (33, 35, 41, 43) or (op == 58 and w & 3 == 1):
            put(ra, None)
    elif op == 46:  # lmw
        state = {r: v for r, v in state.items() if r < rt}
    elif op in (20, 21, 23, 26, 27, 28, 29):
        put(ra, None)
    elif op in (37, 39, 45, 49, 51, 53, 55) or (op == 62 and w & 3 == 1):
        put(ra, None)
    elif op not in (10, 11, 16, 18, 19, 36, 38, 44, 47, 48, 50, 52, 54, 59, 62, 63):
        state.clear()
    if (op in (16, 18, 19) and w & 1):
        state = {r: v for r, v in state.items() if r >= 14}
    return state


def seed_analyze(image, start):
    """Reachability and fixed-point must analysis, with intersection at joins."""
    end = image.ends[start]
    states, pending = {start: {3: 'this'}}, deque([start])
    while pending:
        p = pending.popleft()
        w = image.rd(p)
        op = w >> 26
        successors = [p+4]
        if op == 18:
            successors = [p+4 if w & 1 else branch(p, w)]
        elif op == 16:
            successors = [p+4, branch(p, w)]
        elif op == 19 and (w >> 1) & 1023 in (16, 528):
            successors = [p+4] if w & 1 or (w >> 21) & 31 != 20 else []
        new = transfer(w, states[p])
        for q in successors:
            if not start <= q < end or (q != start and q in image.fset):
                continue
            merged = new if q not in states else {r: v for r, v in states[q].items() if new.get(r) == v}
            if q not in states or merged != states[q]:
                states[q] = dict(merged)
                pending.append(q)
    calls, stores, strings, this_calls = [], [], {}, []
    for p, state in sorted(states.items()):
        w = image.rd(p)
        op, rt, ra = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op in (16, 18):
            target = branch(p, w)
            if target in image.fset and (w & 1 or not start <= target < end):
                calls.append((p, target))
                if state.get(3) == 'this':
                    this_calls.append((p, target))
        if op == 36 and state.get(ra) == 'this' and w & 0xFFFF == 0:
            value = state.get(rt)
            if value in image.tables:
                stores.append((p, value))
        if op in (14, 15, 24, 25):
            out = transfer(w, state)
            reg = ra if op in (24, 25) else rt
            value = out.get(reg)
            if isinstance(value, int):
                text = image.string(value)
                if text:
                    strings[p] = (value, text)
    return {'reachable_bytes': 4*len(states), 'calls': calls,
            'this_calls': this_calls, 'stores': stores, 'strings': strings}


def pc_index(inputs, global_methods):
    """Index actual class declarations and qualified definitions; no stem guess."""
    classes, methods = defaultdict(set), defaultdict(set)
    for path in sorted((ROOT/'research/pvz-pc').rglob('*')):
        if path.suffix not in ('.cpp', '.h'):
            continue
        text = inputs.read(path).decode('utf-8-sig', errors='replace')
        # Mask comments and literals before matching declaration/definition tokens.
        clean = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                       lambda m: ' ' * len(m[0]), text, flags=re.S)
        prefix = 'Sexy::' if path.parent.name == 'SexyAppFramework' else ''
        rel = path.relative_to(ROOT).as_posix()
        for m in re.finditer(r'\b(?:class|struct)\s+(\w+)\s*(?:final\s*)?(?::[^;{}]*)?\{', clean):
            classes[prefix+m[1]].add(rel)
        # Exclude call expressions: require a definition body after the signature.
        for m in re.finditer(r'\b((?:\w+::)+)(~?\w+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:\:[^{};]*)?\{', clean):
            cls = m[1][:-2]
            if cls == 'Sexy':
                methods[('<global>', m[2])].add(rel)
                continue
            if '::' not in cls and prefix:
                cls = prefix+cls
            methods[(cls, m[2])].add(rel)
            classes[cls].add(rel)
        if path.suffix == '.cpp':
            # Search literal accepted names, not an ambiguous return-type regex
            # over millions of characters (quadratic on long whitespace runs).
            for name in global_methods:
                if re.search(r'\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{', clean):
                    methods[('<global>', name)].add(rel)
    return classes, methods


def classify(image, features, names):
    hosts, contexts = defaultdict(set), defaultdict(list)
    for vt, (cls, offset, slots, td, col) in sorted(image.tables.items()):
        for slot, target in enumerate(slots):
            hosts[target].add(cls)
            contexts[target].append({'class': cls, 'vtable': hx(vt), 'slot': slot,
                                     'this_adjustment': offset, 'type_descriptor': hx(td), 'col': hx(col)})
    result = {}
    for a in image.funcs:
        owner = image.owner(hosts[a])
        evidence = {}
        cls, confidence, source = 'unknown', 'unknown', 'none'
        accepted = names.get(a)
        # An exact tail alias can also be a global runtime entry (e.g. delete
        # used as an optimized virtual destructor). Its vtable host alone does
        # not own the shared implementation. Keep evidence, not a high class.
        first = image.rd(a)
        if owner and first >> 26 == 18 and not first & 1:
            target = branch(a, first)
            if target in image.fset and image.owner(hosts[target]) != owner:
                evidence['shared_tail_alias'] = {'target': hx(target), 'vtable_hosts': contexts[a]}
                owner = None
        if owner and not owner.startswith('msvc:'):
            cls, confidence, source = owner, 'high', 'xbox-rtti-vtable'
            evidence['vtable_hosts'] = contexts[a]
        if accepted:
            ac, _ = split_name(accepted['name'])
            if ac == 'Sexy':  # Namespace utilities are free functions, not a class Sexy.
                ac = ''
            ac = ac or '<global>'
            if ac:
                if cls != 'unknown' and ac != cls:
                    evidence['rtti_owner_disagreement'] = cls
                if cls != ac:
                    cls, confidence = ac, accepted['confidence']
                source = 'accepted-name' if not owner or owner != ac else 'xbox-rtti-vtable+accepted-name'
                evidence['accepted'] = {'name': accepted['name'], 'confidence': accepted['confidence'],
                                        'evidence': json.loads(accepted['evidence'])}
        if cls == 'unknown' and region(a) in ('game', 'sexy'):
            stores = features[a]['stores']
            types = {image.tables[vt][0] for _, vt in stores}
            # Constructor/destructor may install several base vtables; retain the
            # most-derived only if all written types are in its ancestry.
            derived = [c for c in types if types <= image.bases.get(c, set()) | {c}]
            if len(derived) == 1 and not derived[0].startswith('msvc:'):
                cls, confidence, source = derived[0], 'medium', 'entry-r3-vtable-store'
                evidence['vtable_stores'] = [{'instruction': hx(p), 'vtable': hx(v), 'class': image.tables[v][0]}
                                             for p, v in stores]
        if cls == 'unknown' and hosts[a]:
            evidence['ambiguous_vtable_hosts'] = sorted(hosts[a])
        result[a] = {'class': cls, 'confidence': confidence, 'source': source, 'evidence': evidence}
    return result


def save_helper(image, address):
    """Prove a straight-line store-only register-save routine from Xbox words.

    Entry points inside the shared sequence are permitted. No named address
    whitelist and no assumption that an ordinary callee preserves r3.
    """
    for p in range(address, address + 32*4, 4):
        w = image.rd(p)
        if w == 0x4E800020:
            return p > address
        if not (w >> 26 in (36, 54, 62) and ((w >> 16) & 31 == 1 or
                w >> 26 == 54 and (w >> 16) & 31 == 12)
                and (w >> 26 != 62 or w & 3 == 0)):
            return False
    return False


def pointer(value):
    return isinstance(value, tuple) and value[0] in ('this', 'field')


def advance(value, delta):
    if isinstance(value, int):
        return (value + delta) & 0xFFFFFFFF
    if pointer(value):
        return (*value[:-1], value[-1] + delta)
    return None


def flow_transfer(image, p, old, helpers):
    w = image.rd(p)
    op, rt, ra, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 65535
    state = transfer(w, old)
    def put(reg, val):
        state.pop(reg, None)
        if val is not None:
            state[reg] = val
    if op in (14, 15):
        put(rt, advance(0 if ra == 0 else old.get(ra),
                        signed(imm, 16) * (65536 if op == 15 else 1)))
    elif op == 32 and ra and pointer(old.get(ra)):
        # A field expression carries the exact load instruction and full base
        # expression. Only the explicit T04 whitelist below may resolve it.
        val = old[ra]
        depth = 0
        while val[0] == 'field':
            depth += 1
            val = val[1]
        if depth < 3:
            put(rt, ('field', old[ra], signed(imm, 16), p, 0))
    if op == 18 and w & 1 and branch(p, w) in helpers:
        state = dict(old)
    return state


def analyze(image, start, helpers=frozenset()):
    """PPC must analysis: exact r3 origin/offset, no stack/return-value guesses.

    Join is intersection, unsupported writes kill state, indirect jumps end a
    path. Conditional bclr/bcctr retains fallthrough. Store-only save helpers
    preserve GPRs after byte-level validation; all other calls kill r0..r13.
    """
    end = image.ends[start]
    states, pending = {start: {3: ('this', 0)}}, deque([start])
    while pending:
        p = pending.popleft()
        w, successors = image.rd(p), [p+4]
        op = w >> 26
        if op == 18:
            successors = [p+4 if w & 1 else branch(p, w)]
        elif op == 16:
            successors = [p+4] if w & 1 else [p+4, branch(p, w)]
        elif op == 19 and (w >> 1) & 1023 in (16, 528):
            successors = [p+4] if w & 1 or (w >> 21) & 31 != 20 else []
        new = flow_transfer(image, p, states[p], helpers)
        for q in successors:
            if not start <= q < end or (q != start and q in image.fset):
                continue
            merged = new if q not in states else {r: v for r, v in states[q].items() if new.get(r) == v}
            if q not in states or merged != states[q]:
                states[q] = dict(merged)
                pending.append(q)
    calls, fields, constants, indirect, stores, literals, object_stores, call_arguments = [], [], {}, [], [], [], [], []
    for p, state in sorted(states.items()):
        w = image.rd(p)
        op, rt, ra = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        if op in (16, 18):
            target = branch(p, w)
            if target in image.fset and (w & 1 or not start <= target < end):
                calls.append((p, target, state.get(3)))
                call_arguments.append((p, target, {r: state[r] for r in range(4, 11) if isinstance(state.get(r), int)}))
        if op == 19 and (w >> 1) & 1023 == 528:
            indirect.append(p)
        if op == 36 and state.get(ra) == ('this', 0) and w & 65535 == 0:
            value = state.get(rt)
            if isinstance(value, int) and value in image.tables:
                stores.append((p, value))
        if op == 36 and pointer(state.get(ra)):
            object_stores.append((p, state[ra], signed(w & 65535, 16), state.get(rt)))
        if op in (32, 34, 36, 38, 40, 42, 44, 48, 50, 52, 54) and ra:
            origin = state.get(ra)
            if pointer(origin) and origin[0] == 'this':
                fields.append((p, origin[-1] + signed(w & 65535, 16)))
        if op in (14, 15, 24, 25):
            out = flow_transfer(image, p, state, helpers)
            value = out.get(ra if op in (24, 25) else rt)
            if isinstance(value, int) and 255 < value < 0x100000:
                constants[p] = value
            if isinstance(value, int) and BASE <= value < BASE+len(image.raw):
                literals.append((p, value))
    return {'reachable_bytes': len(states)*4, 'calls': [(p, t) for p, t, _ in calls],
            'call_origins': calls, 'fields': fields, 'constants': constants,
            'indirect': indirect, 'stores': stores, 'literals': literals, 'object_stores': object_stores,
            'call_arguments': call_arguments,
            'save_calls': [(p, t) for p, t, _ in calls if t in helpers]}


# Field TYPE facts only. All offsets, pointer-vs-inline/array shape and evidence
# come from the accepted Xbox T04 CSV. Unknown pointer semantics are not inferred.
# In particular: no generic Widget.parent/children cast to a most-derived type.
FIELD_TYPES = {
    ('LawnApp', 'board'): 'Board', ('LawnApp', 'main_menu'): 'MainMenu',
    ('LawnApp', 'challenge_screen'): 'ChallengeScreen',
    ('LawnApp', 'widget_manager'): 'Sexy::WidgetManager',
    ('Board', 'app'): 'LawnApp', ('Board', 'seed_banks'): 'SeedBank',
    ('Board', 'controls'): 'GamepadControls', ('Board', 'cursors'): 'CursorObject',
    ('Board', 'challenge'): 'Challenge', ('SeedPacket', 'seed_bank'): 'SeedBank',
    **{(c, f): t for c in ('BaseGamepadControls', 'GamepadControls', 'SeedBank',
                         'SeedPacket', 'Plant', 'Zombie', 'Coin', 'Challenge', 'CursorObject')
       for f, t in (('app', 'LawnApp'), ('board', 'Board'))},
}


def layout_index(layouts):
    result = {}
    for row in layouts:
        cls = 'Sexy::Widget' if row['class'] == 'Widget' else row['class']
        typ = FIELD_TYPES.get((cls, row['field']))
        if not typ:
            continue
        assert row['type'].startswith('GuestPtr')
        count = re.search(r'\[(\d+)\]', row['type'])
        for i in range(int(count[1]) if count else 1):
            result[(cls, int(row['offset'], 16)+i*4)] = (typ, row, i)
    return result


def adjustments(seed):
    hosts = seed['evidence'].get('vtable_hosts', [])
    values = {c['this_adjustment'] for c in hosts if c['class'] == seed['class']}
    return values or {0}


def resolve_origin(origin, caller, fields):
    if not pointer(origin) or caller['class'] in ('unknown', '<global>'):
        return None
    if origin[0] == 'this':
        adj = adjustments(caller)
        if len(adj) != 1:
            return None
        offset = next(iter(adj)) + origin[-1]
        # A pointer to an interior subobject is not the complete object.
        return (caller['class'], [], offset)
    _, base, offset, p, extra = origin
    resolved = resolve_origin(base, caller, fields)
    if resolved is None:
        return None
    cls, path, adjustment = resolved
    field = fields.get((cls, adjustment+offset))
    if field is None:
        return None
    typ, row, index = field
    return typ, path + [{'instruction': hx(p), 'owner': cls, 'field': row['field'],
                         'offset': hex(adjustment+offset), 'element': index, 'type': typ,
                         'layout_instruction': row['evidence_address'],
                         'layout_function': row['evidence_function']}], extra


def extend_classes(image, features, seeds, layouts, baseline=None, libraries=None):
    """Unique multi-offset signatures, then all-call-site closure.

    All direct callers (including untyped and non-this contexts) are counted.
    Low never bootstraps medium. A signature contradicted by a typed call is
    removed and closure restarted; seeds are immutable. No address proximity.
    """
    import copy
    baseline = seeds if baseline is None else baseline
    libraries = libraries or {}
    fields = layout_index(layouts)
    widths = {32: 4, 34: 1, 36: 4, 38: 1, 40: 2, 42: 2, 44: 2,
              48: 4, 50: 8, 52: 4, 54: 8}
    def access(p, off):
        return off, widths.get(image.rd(p) >> 26, 0)
    incoming = defaultdict(list)
    for a, f in features.items():
        for p, target, origin in f['call_origins']:
            if a != target:  # recursion is not an independent caller
                incoming[target].append((a, p, origin))
    signatures = defaultdict(set)
    witnesses = defaultdict(lambda: defaultdict(list))
    for row in layouts:
        cls = 'Sexy::Widget' if row['class'] == 'Widget' else row['class']
        off = int(row['offset'], 16)
        key = access(int(row['evidence_address'], 16), off)
        if off > 0x20 and key[1]:
            signatures[cls].add(key)
            witnesses[cls][key].append({'layout_instruction': row['evidence_address'],
                                         'layout_function': row['evidence_function']})
    for a, seed in seeds.items():
        if a in libraries or seed['class'] in ('unknown', '<global>') or adjustments(seed) != {0}:
            continue
        for p, off in features[a]['fields']:
            if off > 0x20:
                key = access(p, off)
                signatures[seed['class']].add(key)
                witnesses[seed['class']][key].append({'function': hx(a), 'instruction': hx(p)})
    suggestions = {}
    for a, f in features.items():
        if a in libraries or seeds[a]['class'] != 'unknown' or region(a) not in ('game', 'sexy'):
            continue
        accesses = {access(p, off) for p, off in f['fields'] if off > 0x20}
        offsets = {off for off, _ in accesses}
        if len(offsets) < 2:
            continue
        matches = [c for c, sig in signatures.items() if accesses <= sig]
        if len(matches) == 1:
            cls = matches[0]
            suggestions[a] = {'class': cls, 'confidence': 'low', 'source': 'unique-field-signature',
                              'evidence': {'field_signature': {'offsets': sorted(offsets),
                                  'instructions': [{'instruction': hx(p), 'offset': hex(o)}
                                                   for p, o in f['fields'] if o in offsets],
                                  'access_widths': sorted(accesses),
                                  'references': {hex(o)+'/'+str(width): witnesses[cls][(o, width)][:2]
                                                 for o, width in sorted(accesses)},
                                  'unique_among': len(signatures)}}}
    banned, rounds = set(), 0
    while True:
        result = copy.deepcopy(seeds)
        for a, s in suggestions.items():
            if a not in banned:
                result[a] = copy.deepcopy(s)
            else:
                result[a] = {'class': 'unknown', 'confidence': 'unknown', 'source': 'conflict',
                             'evidence': {'signature_class': s['class'], 'typed_calls': []}}
        while True:
            additions = {}
            rounds += 1
            for a in image.funcs:
                if a in libraries or seeds[a]['class'] != 'unknown' or a in banned:
                    continue
                if region(a) not in ('game', 'sexy'):
                    continue
                typed, unresolved = [], []
                for c, p, origin in incoming[a]:
                    resolved = resolve_origin(origin, result[c], fields)
                    if resolved is None:
                        unresolved.append({'caller': hx(c), 'instruction': hx(p), 'origin': origin})
                    else:
                        cls, path, adjustment = resolved
                        # This+N may be a member object. Only an existing RTTI
                        # target with a matching adjustment can prove that cast;
                        # existing targets are frozen seeds, never new labels.
                        if adjustment:
                            unresolved.append({'caller': hx(c), 'instruction': hx(p),
                                               'this_adjustment': adjustment})
                        else:
                            typed.append({'caller': hx(c), 'instruction': hx(p), 'class': cls,
                                          'caller_confidence': result[c]['confidence'],
                                          'origin': origin, 'field_path': path, 'this_adjustment': adjustment})
                owners = {t['class'] for t in typed}
                evidence = {'typed_calls': typed, 'unresolved_calls': unresolved,
                            'static_call_sites': len(incoming[a]), 'round': rounds}
                if a in suggestions:
                    cls = suggestions[a]['class']
                    # A signature is only low until a separate H/M caller
                    # proves the same r3 type. Low cycles cannot self-confirm.
                    supporting = [t for t in typed if t['class'] == cls and
                                  t['caller_confidence'] in ('high', 'medium')]
                    item = copy.deepcopy(suggestions[a])
                    item['confidence'] = 'medium' if supporting else 'low'
                    item['evidence']['signature_support'] = supporting
                    item['evidence']['round'] = rounds
                elif len(owners) > 1:
                    item = {'class': 'unknown', 'confidence': 'unknown', 'source': 'conflict', 'evidence': evidence}
                elif len(owners) == 1 and not unresolved:
                    strong = all(t['caller_confidence'] in ('high', 'medium') for t in typed)
                    # A verified pointer load is a separate class witness, even
                    # with one caller. Identity alone needs two distinct methods.
                    medium = strong and (len({t['caller'] for t in typed}) >= 2 or any(t['field_path'] for t in typed))
                    item = {'class': next(iter(owners)), 'confidence': 'medium' if medium else 'low',
                            'source': 'all-static-r3-contexts', 'evidence': evidence}
                else:
                    item = {'class': 'unknown', 'confidence': 'unknown',
                            'source': 'unresolved-callers' if incoming[a] else
                                      'no-static-callers-indirect-possible', 'evidence': evidence}
                prev = result[a]
                if (item['class'], item['confidence']) != (prev['class'], prev['confidence']):
                    additions[a] = item
                else:
                    result[a] = item
            if not additions:
                break
            result.update(additions)
        contradictions = set()
        for a in suggestions.keys() - banned:
            for c, p, origin in incoming[a]:
                value = resolve_origin(origin, result[c], fields)
                if value and value[2] == 0 and value[0] != result[a]['class']:
                    contradictions.add(a)
        if not contradictions:
            break
        banned.update(contradictions)
    for a, item in result.items():
        if a in libraries:
            if item['class'] == 'unknown':
                item['source'] = 'middleware-library'
            item['evidence']['library'] = libraries[a]
        item['evidence']['t11_seed'] = baseline[a]['class'] != 'unknown'
        if item['source'] == 'unique-field-signature':
            item['evidence']['static_call_contexts'] = [
                {'caller': hx(c), 'instruction': hx(p), 'origin': origin,
                 'resolved': resolve_origin(origin, result[c], fields)} for c, p, origin in incoming[a]]
        if a in banned:
            item['evidence']['rejected_signature'] = suggestions[a]
            item['evidence']['typed_calls'] = [
                {'caller': hx(c), 'instruction': hx(p), 'resolved': resolve_origin(origin, result[c], fields)}
                for c, p, origin in incoming[a]]
        if item['class'] == 'unknown':
            if a in libraries:
                reason = 'middleware-library'
            elif item['source'] == 'conflict':
                reason = 'conflict'
            elif not incoming[a]:
                reason = 'no-static-callers / indirect-target-possible' if image.refs.get(a) else 'no-callers'
            elif all(origin is None for _, _, origin in incoming[a]):
                reason = 'r3-origin-lost-or-non-object'
            else:
                reason = 'unresolved-callers-or-this-adjustment'
            item['evidence']['unassigned_reason'] = reason
    return result, {'rounds': rounds, 'signatures_proposed': len(suggestions),
                    'signatures_rejected': len(banned)}


def library_regions(image, features, seeds, names):
    """Find embedded C libraries from Xbox names/diagnostics and forward closure.

    Accepted member classes are barriers. Edges are reachable direct calls,
    literal callback pointers and short readonly callback tables. Only callers
    passing the unchanged object can extend closure backwards; no address gaps
    are filled. Diagnostic string tables (JPEG)
    supply roots even when messages are selected by numeric error codes.
    """
    def family(s):
        if re.search(r'(?i)\b(?:deflate|inflate) 1\.1\.3 Copyright', s):
            return 'zlib'
        if re.search(r'JPEG-2000|ISO/IEC 15444', s):
            return 'JPEG-2000'
        if re.search(r'(?i)JPEG|JFIF|Huffman|quantization', s):
            return 'JPEG'
        if re.search(r'(?i)libpng|read_data_fn|write_data_fn|\b(?:PNG|IHDR|PLTE|IDAT|zTXt)\b', s):
            return 'libpng'
        if re.search(r'(?i)zlib (?:error|memory|version)', s):
            return 'libpng/zlib'
        return None
    strings = {}
    for m in re.finditer(rb'[ -~]{5,}(?:\n[ -~]*)?', image.raw):
        text = m[0].decode('ascii')
        lib = family(text)
        if lib:
            strings[BASE+m.start()] = (lib, text)
    tables = {}
    for addr, (lib, _) in sorted(strings.items()):
        for ref in image.refs[addr]:
            lo, hi = ref, ref+4
            while ref-lo < 1024 and image.string(image.rd(lo-4)):
                lo -= 4
            while hi-ref < 1024 and image.string(image.rd(hi)):
                hi += 4
            hits = [(p, image.rd(p)) for p in range(lo, hi, 4) if image.rd(p) in strings and strings[image.rd(p)][0] == lib]
            if len(hits) >= 2:
                tables[(lo, hi, lib)] = hits
    roots = {}
    eligible = {a for a in image.funcs if region(a) == 'sexy' and seeds[a]['class'] in ('unknown', '<global>')}
    for a in sorted(eligible):
        named = names.get(a, {}).get('name', '')
        lib = ('libpng' if named.startswith('png_') else 'JPEG' if named.startswith('jpeg_')
               else 'zlib' if re.match(r'(?:deflate|inflate|adler32|crc32|compress|uncompress)\b', named) else None)
        if lib:
            roots[a] = {'family': lib, 'kind': 'accepted-name', 'name': named}
        # Xbox JPEG error protocol, corroborated by its diagnostic table:
        # entry->err at +0, then err->msg_code at +14, integer error index.
        # Writing a runtime code through [this+0] is incompatible with an
        # immutable C++ vtable; this is not an offset-only class signature.
        jpeg_stores = [(p, origin, value) for p, origin, off, value in features[a].get('object_stores', [])
                       if off == 0x14 and origin[0] == 'field' and origin[1] == ('this', 0) and
                       origin[2] == 0 and origin[-1] == 0 and isinstance(value, int) and 0 < value < 256]
        if jpeg_stores and any(lib == 'JPEG' for _, _, lib in tables):
            p, origin, value = jpeg_stores[0]
            roots.setdefault(a, {'family': 'JPEG', 'kind': 'jpeg-error-protocol', 'instruction': hx(p),
                                  'error_pointer_load': hx(origin[3]), 'error_index': value,
                                  'diagnostic_tables': [hx(lo) for lo, _, lib in tables if lib == 'JPEG']})
        adler = [p for p, v in features[a]['constants'].items() if v == 65521]
        if adler and any((image.rd(p) >> 26 == 31 and (image.rd(p) >> 1) & 1023 == 459)
                         for p in range(a, image.ends[a], 4)):
            roots.setdefault(a, {'family': 'zlib', 'kind': 'adler-modulus-division', 'instruction': hx(adler[0]),
                                  'modulus': 65521, 'copyrights': [{'address': hx(v), 'string': s}
                                      for v, (lib, s) in strings.items() if lib == 'zlib']})
        for p, value in features[a].get('literals', []):
            if value in strings:
                lib, s = strings[value]
                roots.setdefault(a, {'family': lib, 'kind': 'diagnostic-string', 'instruction': hx(p),
                                      'string_address': hx(value), 'string': s})
            for (lo, hi, lib), hits in tables.items():
                if lo <= value < hi:
                    roots.setdefault(a, {'family': lib, 'kind': 'diagnostic-table', 'instruction': hx(p),
                                          'table_start': hx(lo), 'table_end': hx(hi),
                                          'examples': [{'entry': hx(q), 'string_address': hx(v), 'string': strings[v][1]}
                                                       for q, v in hits[:3]]})
    # Xbox PNG init calls expose the zlib ABI independently: a literal version
    # 1.1.3 and sizeof(z_stream)=0x38 passed to a direct target. The caller may
    # be PNG; only the called init function is made a zlib root.
    for a in sorted(roots):
        for p, target, arguments in features[a].get('call_arguments', []):
            versions = [(r, v) for r, v in arguments.items() if BASE <= v < BASE+len(image.raw) and
                        image.raw[v-BASE:v-BASE+6] == b'1.1.3\0']
            if target in eligible and versions and 0x38 in arguments.values():
                roots[target] = {'family': 'zlib', 'kind': 'versioned-zstream-init', 'instruction': hx(p),
                                 'caller': hx(a), 'version_arguments': [(r, hx(v)) for r, v in versions],
                                 'stream_size': 0x38,
                                 'copyrights': [{'address': hx(v), 'string': s}
                                               for v, (lib, s) in strings.items() if lib == 'zlib']}
    libraries = dict(roots)
    incoming_identity = defaultdict(list)
    for c in sorted(eligible):
        for p, t, origin in features[c]['call_origins']:
            if origin == ('this', 0):
                incoming_identity[t].append((p, c))
    pending = deque(sorted(roots))
    while pending:
        a = pending.popleft()
        edges = [(p, t, 'direct-call') for p, t in features[a]['calls']]
        for p, value in features[a].get('literals', []):
            if value in image.fset:
                edges.append((p, value, 'callback-literal'))
            elif value < image.funcs[0]:
                # A readonly run of code pointers; a single arbitrary word is
                # insufficient. RTTI-owned member targets remain barriers.
                targets = []
                for q in range(value, value+256, 4):
                    t = image.rd(q)
                    if t not in image.fset:
                        break
                    targets.append(t)
                if len(targets) >= 2:
                    edges.extend((p, t, 'callback-table') for t in targets)
        for p, t, kind in sorted(edges):
            if t in eligible and t not in libraries:
                libraries[t] = {'family': libraries[a]['family'], 'kind': kind,
                                'from': hx(a), 'instruction': hx(p)}
                pending.append(t)
        for p, caller in incoming_identity[a]:
            if caller not in libraries:
                libraries[caller] = {'family': libraries[a]['family'], 'kind': 'library-object-caller',
                                     'callee': hx(a), 'instruction': hx(p)}
                pending.append(caller)
    # Mathematical checksum fingerprints label zlib helpers inside the already
    # proven library closure; they never extend the middleware domain alone.
    for a in sorted(libraries):
        constants = {v for _, v in features[a].get('literals', [])} | set(features[a]['constants'].values())
        if 65521 in constants:  # Adler modulus, separately visible in PPC
            libraries[a] = {**libraries[a], 'family': 'zlib', 'checksum_modulus': 65521}
    zones = []
    for a in image.funcs:
        if a not in libraries:
            continue
        lib = libraries[a]['family']
        if zones and zones[-1]['end'] == hx(a) and zones[-1]['family'] == lib:
            zones[-1]['end'] = hx(image.ends[a])
            zones[-1]['functions'] += 1
        else:
            zones.append({'start': hx(a), 'end': hx(image.ends[a]), 'family': lib, 'functions': 1,
                          'first_evidence': libraries[a]})
    return libraries, {'roots': {hx(a): r for a, r in sorted(roots.items())}, 'zones': zones,
                        'families': dict(Counter(v['family'] for v in libraries.values()))}


def csv_bytes(rows, fields):
    out = io.StringIO(newline='')
    writer = csv.DictWriter(out, fieldnames=fields, lineterminator='\n')
    writer.writeheader()
    writer.writerows(rows)
    return out.getvalue().encode('utf-8')


# Subsystem policy and checks; this never supplies additional class evidence.
BATCH_POLICY = [
    ('Данные и буферы', r'Buffer|DataReader|DataWriter|DataSync|DataElement|RefCount|EncodingParser|SyncObject', 'Побайтные round-trip, endian, границы и ошибочный ввод.'),
    ('XML и ресурсы', r'XMLParser|PropertiesParser|DescParser|ResourceManager|CfgCompiler', 'Порядок загрузки, отсутствующие ресурсы, локализации en/de/es/fr/it/jp.'),
    ('Ядро Widget', r'Sexy::Widget(?:Container|Manager)?$', 'Жизненный цикл, фокус, маршрутизация; menu_hover.csv, input_ownership.csv.'),
    ('Кнопки и списки', r'Button|Checkbox|Slider|Scroll|EditWidget|LabelWidget', 'Наведение/нажатие/отмена, клавиатура; menu_panels.csv, game_settings.csv.'),
    ('Парсер меню', r'MenuParser|MenuWidget|WidgetAnimator', 'Порядок анимаций, переходы меню и resize; menu_panels_actions.csv.'),
    ('Базовый ввод', r'BaseGamepadControls|Sexy::Gamepad', 'Геймпад/мышь, дрейф и повтор; input_ownership.csv; физический контроллер.'),
    ('Малые объекты поля', r'Coin|Projectile|LawnMower|GridItem|GameObject|CursorObject|CursorPreview|TrashBin', 'Сбор, снаряды, газонокосилка, лопата; cursor_modes.csv, adventure_mouse.csv.'),
    ('Plant', r'^Plant$', 'RNG, порядок обновления/атаки, состояния растения; снимки T05/T06.'),
    ('Zombie', r'^Zombie$', 'RNG, движение, урон и смерть всех типов; снимки T05/T06.'),
    ('Семена и банк', r'SeedPacket|SeedBank', 'Цена, cooldown, лента; keyboard_seeds.csv, level_10_mouse.csv.'),
    ('Board', r'^Board$', 'Шаги симуляции, пауза, Co-op/Versus; adventure_mouse.csv, cursor_modes.csv.'),
    ('Challenge', r'^Challenge$', 'Heavy Weapon, I Zombie, Zombiquarium; отдельные снимки каждого режима.'),
    ('Управление игрой', r'GamepadControls|ZenGardenControls|TreeOfWisdomControls', 'Двойной шаг паузы, пропуск клика I Zombie, Heavy Weapon, два игрока.'),
    ('Выбор растений', r'SeedChooserScreen|ImitaterDialog', 'Добавление/возврат карточки и старт; oct7_chooser.csv.'),
    ('Главное меню и титул', r'MainMenu|TitleScreen', 'Trial/Full, профиль, ввод; adventure_entry.csv, menu_panels_actions.csv.'),
    ('Режимы и награды', r'ChallengeScreen|AwardScreen|GameOverDialog|ContinueDialog', 'Замки и награды; award_mouse.csv, выбор каждого режима.'),
    ('Сервисы игры', r'StoreScreen|AlmanacDialog|MailScreen|HouseChooserDialog|DaveWidget', 'Покупки, альманах, сад, почта; сценарии docs/services-mouse.md.'),
    ('Настройки и помощь', r'Dialog|Help|MessageWidget', 'game_settings.csv, все языки, Back/drag/отмена.'),
    ('Versus и Co-op', r'VSSetupMenu|VSResultsMenu', 'Подключение второго игрока, выбор сторон, результаты и повтор матча.'),
    ('Профили и сохранения', r'PlayerInfo|ProfileMgr|ProfileEventListener|Storage|ReadRequest|WriteRequest', 'Round-trip отдельного тестового профиля, ошибки и восстановление сохранения.'),
    ('LawnApp и запуск', r'^LawnApp$|Sexy::SexyApp', 'Загрузка, цикл 10 мс, переходы меню/уровень, завершение и все языки.'),
    ('Анимация и эффекты', r'Reanimation|ReanimTrack|TodParticle|Trail|Attachment|CreditScreen|CreditsOverlay|ImageWidget', 'Порядок слоёв и кадров, эффекты, титры; кадры и данные T05, PS3 .reanim как справочник.'),
    ('Рендер, изображения и шрифты', r'Image|Font|Graphics|D3D|DDInterface|NativeDisplay', 'Zombiquarium/Peashooter, эталонные кадры, alpha/clip/transform, все шрифты.'),
    ('Звук и платформа', r'Music|Sound|FMod|Internet|HTTP|Leaderboard|HouseMenu|IQNet|PakInterface|FEAST::', 'audio_settings_zero.csv, потеря канала, загрузка PAK, платформенные ошибки; физический звук.'),
]


def semantic_area(cls):
    if cls == 'unknown':
        return 'unknown'
    if cls == '<global>':
        return 'game'
    if cls.startswith(('FEAST::', 'ImageLib::')):
        return 'middleware'
    if cls.startswith(('std::', 'msvc:')):
        return 'crt-xdk'
    if re.search(r'Xbox360|InternetManager|HTTPTransfer|Leaderboard|Storage|IQNet|ReadRequest|WriteRequest', cls):
        return 'xbox-platform'
    if cls.startswith('Sexy::') or cls in ('PakInterface', 'FullSizeImage'):
        return 'sexy'
    return 'game'


def strongly_connected(graph):
    index, low, stack, active, components = {}, {}, [], set(), []
    def visit(v):
        index[v] = low[v] = len(index)
        stack.append(v)
        active.add(v)
        for w in sorted(graph[v]):
            if w not in index:
                visit(w)
                low[v] = min(low[v], low[w])
            elif w in active:
                low[v] = min(low[v], index[w])
        if low[v] == index[v]:
            group = []
            while True:
                w = stack.pop()
                active.remove(w)
                group.append(w)
                if w == v:
                    break
            components.append(sorted(group))
    for v in sorted(graph):
        if v not in index:
            visit(v)
    return components


def self_test():
    assert transfer(0x7C7F1B78, {3: 'this'})[31] == 'this'  # mr r31,r3
    assert transfer(0x38630004, {3: 'this'}).get(3) is None
    assert transfer(0x38630000, {3: 'this'})[3] == 'this'
    assert transfer(0x48000001, {3: 'this', 31: 'this'}) == {31: 'this'}
    assert branch(0x82120000, 0x4BFFFFFC) == 0x8211FFFC
    assert transfer(0x3C608205, {})[3] == 0x82050000
    assert transfer(0x3863D74C, {3: 0x82060000})[3] == 0x8205D74C
    # Synthetic diamond: r3 identity is lost on one arm, so must not survive join.
    class Fake:
        funcs = [0x1000]
        fset = set(funcs)
        ends = {0x1000: 0x1020}
        tables = {0x12340000: ('A', 0, [], 0, 0)}
        words = {0x1000: 0x4182000C, 0x1004: 0x38600000, 0x1008: 0x48000008,
                 0x100C: 0x60000000, 0x1010: 0x3C801234, 0x1014: 0x90830000,
                 0x1018: 0x4E800020, 0x101C: 0x48000001}
        def rd(self, a):
            return self.words.get(a, 0)
        def string(self, a):
            return None
    f = seed_analyze(Fake(), 0x1000)
    assert not f['stores'] and f['reachable_bytes'] == 28
    Fake.words[0x1004] = 0x60000000
    assert seed_analyze(Fake(), 0x1000)['stores'] == [(0x1014, 0x12340000)]
    # Inline branch-looking bytes after blr must not create calls.
    assert not f['calls']
    assert sorted(strongly_connected({'a': {'b'}, 'b': {'a'}, 'c': set()})) == [['a', 'b'], ['c']]
    assert split_name('Sexy::A::f(B::C*)') == ('Sexy::A', 'f(B::C*)')
    # The vtable host of an alias to a global runtime function is not an owner.
    class Alias:
        funcs = [0x1000, 0x1008]
        fset = set(funcs)
        tables = {0x1234: ('A', 0, [0x1000], 0, 0)}
        bases = {'A': set()}
        owner = Image.owner
        def rd(self, a):
            return 0x48000008 if a == 0x1000 else 0x4E800020
    alias = classify(Alias(), {a: {'stores': [], 'this_calls': []} for a in Alias.funcs}, {})
    assert alias[0x1000]['class'] == 'unknown'
    # T11b: offsets survive arithmetic; field loads carry exact instruction.
    assert advance(('this', 0), 4) == ('this', 4)
    assert flow_transfer(Fake(), 0x1004, {3: ('this', 0)}, set()).get(3) == ('this', 0)
    Fake.words[0x1004] = 0x806301A4  # lwz r3,0x1a4(r3)
    assert flow_transfer(Fake(), 0x1004, {3: ('this', 0)}, set())[3] == ('field', ('this', 0), 0x1A4, 0x1004, 0)
    seed = {'class': 'Board', 'confidence': 'high', 'source': 'test', 'evidence': {}}
    layout = [{'class': 'Board', 'field': 'seed_banks', 'offset': '0x1A4', 'type': 'GuestPtr[2]',
               'evidence_address': '0x1234', 'evidence_function': 'Board::Board'}]
    fld = layout_index(layout)
    assert resolve_origin(('field', ('this', 0), 0x1A8, 0x1004, 0), seed, fld)[0] == 'SeedBank'
    assert resolve_origin(('field', ('this', 0), 0x1AC, 0x1004, 0), seed, fld) is None
    adjusted = {'class': 'Board', 'evidence': {'vtable_hosts': [{'class': 'Board', 'this_adjustment': 4}]}}
    assert resolve_origin(('this', -4), adjusted, fld) == ('Board', [], 0)
    assert resolve_origin(('this', 0), adjusted, fld) == ('Board', [], 4)
    adjusted['evidence']['vtable_hosts'].append({'class': 'Board', 'this_adjustment': 0})
    assert resolve_origin(('this', 0), adjusted, fld) is None
    Fake.words[0x2000], Fake.words[0x2004] = 0xFBE1FFF0, 0x4E800020  # std; blr
    assert save_helper(Fake(), 0x2000)
    Fake.words[0x2000] = 0x38600000  # li r3,0 is not a save helper
    assert not save_helper(Fake(), 0x2000)
    Fake.words[0x2000] = 0xDB8CFFE0  # stfd f28,-0x20(r12)
    assert save_helper(Fake(), 0x2000)
    Fake.words[0x2008] = 0x48000FF9  # bl into 3000, not a generated function start
    assert flow_transfer(Fake(), 0x2008, {3: ('this', 0)}, {0x3000})[3] == ('this', 0)
    # Controlled graph: two callers -> medium -> next level; one -> low;
    # untyped caller blocks; two classes conflict; nonzero adjustment blocks.
    class Graph:
        funcs = list(range(0x82120000, 0x82120030, 4))
        refs = {}
        def rd(self, a):
            return 0
    g = Graph()
    gs = {a: {'class': 'unknown', 'confidence': 'unknown', 'source': 'none', 'evidence': {}} for a in g.funcs}
    for i in (0, 1, 2):
        gs[g.funcs[i]] = {'class': 'A' if i < 2 else 'B', 'confidence': 'high', 'source': 'test', 'evidence': {}}
    gf = {a: {'call_origins': [], 'fields': []} for a in g.funcs}
    def edge(c, t, origin=('this', 0)):
        gf[g.funcs[c]]['call_origins'].append((g.funcs[c]+0x100, g.funcs[t], origin))
    edge(0, 3); edge(1, 3); edge(3, 4); edge(0, 4)
    edge(0, 5); edge(0, 6); edge(7, 6)
    edge(0, 8); edge(2, 8); edge(0, 9, ('this', 4))
    result, _ = extend_classes(g, gf, gs, [])
    assert result[g.funcs[3]]['confidence'] == result[g.funcs[4]]['confidence'] == 'medium'
    assert result[g.funcs[5]]['confidence'] == 'low'
    assert result[g.funcs[6]]['class'] == result[g.funcs[9]]['class'] == 'unknown'
    assert result[g.funcs[8]]['source'] == 'conflict'
    edge(5, 10); edge(5, 11); edge(10, 11)
    result, _ = extend_classes(g, gf, gs, [])
    assert result[g.funcs[11]]['confidence'] == 'low'  # low cannot promote itself
    # Same offsets with different widths must not identify a class. A single
    # matching offset is insufficient; a genuine two-field match is evidence.
    g.rd = lambda a: 0x80630040 if a in (0x1000, 0x1004, 0x1008, 0x100C) else 0x88630040
    gf[g.funcs[0]]['fields'] = [(0x1000, 0x40), (0x1004, 0x44)]
    gf[g.funcs[7]]['fields'] = [(0x1010, 0x40), (0x1014, 0x44)]
    result, _ = extend_classes(g, gf, gs, [])
    assert result[g.funcs[7]]['class'] == 'unknown'
    gf[g.funcs[7]]['fields'] = [(0x1008, 0x40)]
    result, _ = extend_classes(g, gf, gs, [])
    assert result[g.funcs[7]]['class'] == 'unknown'
    gf[g.funcs[7]]['fields'].append((0x100C, 0x44))
    result, _ = extend_classes(g, gf, gs, [])
    assert result[g.funcs[7]]['class'] == 'A'
    assert result[g.funcs[7]]['confidence'] == 'low'
    edge(0, 7)
    result, _ = extend_classes(g, gf, gs, [])
    assert result[g.funcs[7]]['confidence'] == 'medium'
    assert result[g.funcs[7]]['evidence']['signature_support'][0]['class'] == 'A'
    edge(2, 7)  # signature A vs independently typed caller B: stay conflict
    result, _ = extend_classes(g, gf, gs, [])
    assert result[g.funcs[7]]['source'] == 'conflict' and result[g.funcs[7]]['class'] == 'unknown'
    result, _ = extend_classes(g, gf, gs, [], libraries={g.funcs[7]: {'family': 'test-C-library'}})
    assert result[g.funcs[7]]['class'] == 'unknown' and result[g.funcs[7]]['source'] == 'middleware-library'
    print('self-test: legacy CFG/RTTI checks and T11b provenance/closure/conflict checks passed')


def confirm_methods(image, features, classified, names, symbols, pc_methods):
    """Search same-class Android bodies using named callees AND immediates.

    Xbox labels are already frozen. Two distinct accepted named callees and
    two nontrivial constants, unique within the class, are required. PC source
    definitions corroborate a resulting name, never an Xbox class.
    """
    sys.path.insert(0, str(ROOT/'scripts'))
    from name_xbox_from_android import Android
    from capstone.arm import ARM_OP_IMM
    android = Android(symbols)
    by_class = defaultdict(list)
    for sym in symbols:
        if sym['type'] == 2 and sym['section'] and sym['size']:
            cls, _ = split_name(sym['demangled'])
            by_class[cls].append(sym)
    canonical = lambda s: s.split('(')[0]
    accepted = {a: canonical(r['name']) for a, r in names.items()}
    cache, matches = {}, {}
    searched = Counter()
    for a, item in classified.items():
        if a in names or item['confidence'] not in ('high', 'medium'):
            continue
        searched['eligible_xbox_functions'] += 1
        named_calls = {accepted[t] for _, t in features[a]['calls'] if t in accepted}
        constants = set(features[a]['constants'].values())
        if len(named_calls) < 2:
            searched['insufficient_named_callees'] += 1
            continue
        if len(constants) < 2:
            searched['insufficient_constants'] += 1
            continue
        candidates = []
        for sym in by_class[item['class']]:
            b = sym['value'] & ~1
            if b not in cache:
                calls = {canonical(android.name(t)) for t in android.calls(b) if android.name(t)}
                immediates = set()
                code = android.data[android.off(b):android.off(b)+sym['size']]
                for ins in android.cs.disasm(code, b):
                    # Load/store displacements are layouts, not semantic constants.
                    if ins.mnemonic.split('.')[0] not in ('mov', 'movw', 'cmp', 'add', 'sub', 'rsb'):
                        continue
                    for operand in ins.operands:
                        if operand.type == ARM_OP_IMM and 255 < operand.imm < 0x100000:
                            immediates.add(operand.imm)
                cache[b] = calls, immediates
            common_calls = named_calls & cache[b][0]
            common_constants = constants & cache[b][1]
            if len(common_calls) >= 2 and len(common_constants) >= 2:
                candidates.append((sym, common_calls, common_constants))
        searched['xbox_functions'] += 1
        searched['android_candidates'] += len(by_class[item['class']])
        unique = {s['demangled'] for s, _, _ in candidates}
        if len(unique) == 1:
            sym, calls, constants = candidates[0]
            _, method = split_name(sym['demangled'])
            matches[a] = {'name': sym['demangled'], 'confidence': 'medium',
                          'evidence': {'android_address': hx(sym['value'] & ~1),
                              'named_calls': sorted(calls), 'constants': sorted(constants),
                              'xbox_constant_instructions': {hx(p): v for p, v in features[a]['constants'].items() if v in constants},
                              'xbox_call_instructions': [{'instruction': hx(p), 'target': hx(t), 'name': accepted[t]}
                                                         for p, t in features[a]['calls'] if accepted.get(t) in calls],
                              'pc_definitions': sorted(pc_methods.get((item['class'], method.split('(')[0]), []))}}
    # ICF and constructor aliases can otherwise give the same proposed name to
    # multiple Xbox implementations. Ambiguous proposals remain unnamed.
    counts = Counter(r['name'] for r in matches.values())
    matches = {a: r for a, r in matches.items() if counts[r['name']] == 1}
    return matches, dict(searched)


def plan_batches(rows, features, inputs):
    for path in ('docs/native-port/agent-guide.md', 'docs/native-port/tasks/T11b-class-coverage.md',
                 'docs/native-port/review.md', 'docs/native-port/README.md',
                 'docs/native-port-feasibility.md', 'docs/technical-notes.md',
                 'docs/cross-platform-research.md', 'docs/services-mouse.md',
                 'docs/native-port/reports/T11-review.md', 'docs/native-port/reports/T08.md'):
        inputs.read(path)
    groups = defaultdict(list)
    def risk_for(cls):
        return (3 if re.search(r'Music|Sound|FMod|Xbox|Internet|HTTP|Leaderboard|Storage|IQNet|PakInterface|FEAST::|ReadRequest|WriteRequest|HouseMenu', cls)
                else 2 if re.search(r'Image|Font|Graphics|D3D|DDInterface|NativeDisplay|Reanim|Particle|Trail|Credit', cls)
                else 0)
    def policy_for(cls):
        return next(((title, checks) for title, pattern, checks in BATCH_POLICY if pattern and re.search(pattern, cls)),
                    (cls, 'T10: ABI, дифференциальные вызовы, снимки затронутого сценария.'))
    for row in rows:
        cls = row['class']
        row['semantic_area'] = semantic_area(cls)
        if row.get('library'):
            row['semantic_area'] = 'middleware'
        if row['name'].startswith('Sexy::') and cls == '<global>':
            row['semantic_area'] = 'sexy'
        if row['method'].startswith(('png_', 'jpeg_')):
            row['semantic_area'] = 'middleware'
        eligible = row['region'] in ('game', 'sexy') or (
            row['semantic_area'] in ('game', 'sexy') and row['class_confidence'] in ('high', 'medium'))
        if row['semantic_area'] in ('crt-xdk', 'middleware'):
            eligible = False
        if not eligible or cls in ('unknown', '<global>'):
            row['assignment'] = 'unassigned' if eligible else 'out-of-scope'
            continue
        row['assignment'] = 'batch'
        group = cls if cls != '<global>' else row['semantic_area']+' namespace utilities'
        groups[group].append(row)
    # Small related classes form real subsystems (never an unknown/address bin).
    merged = defaultdict(list)
    for cls, group in sorted(groups.items()):
        key = (cls if len(group) >= 20 else policy_for(cls)[0]+f' [risk {risk_for(cls)}]')
        merged[key].extend(group)
    groups = merged
    batches = []
    for cls, rs in sorted(groups.items()):
        # Split large classes into balanced chunks <=150. Small classes stay
        # intact: inventing subsystem ownership to meet 20 would hide evidence.
        count = (len(rs)+149)//150
        chunk_size = (len(rs)+count-1)//count
        rs = sorted(rs, key=lambda r: (bool(features[int(r['address'], 16)]['calls']), int(r['address'], 16)))
        for i in range(0, len(rs), chunk_size):
            group = rs[i:i+chunk_size]
            leaf = sum(not features[int(r['address'], 16)]['calls'] and
                       not features[int(r['address'], 16)]['indirect'] for r in group)
            hooked = sum(bool(r['port_hooks']) for r in group)
            pc = sum(bool(r['pc_class_files']) for r in group)
            classes = sorted({r['class'] for r in group})
            risk = max(risk_for(c) for c in classes)
            policy = policy_for(classes[0])
            title, checks = policy
            if hooked:
                checks += ' Хуки: '+ '; '.join(sorted({r['port_hooks'] for r in group if r['port_hooks']}))+'.'
            batches.append({'batch': '', 'order': 0, 'title': cls + (f' / {i//chunk_size+1}' if count > 1 else ''),
                            'subsystem': title, 'functions': len(group), 'bytes': sum(r['bytes'] for r in group),
                            'leaf_functions': leaf, 'leaf_fraction': f'{leaf/len(group):.6f}',
                            'pc_classes': pc, 'pc_methods': sum(bool(r['pc_method_files']) for r in group),
                            'hooked': hooked, 'risk': risk,
                            'unknown': 0, 'low': sum(r['class_confidence'] == 'low' for r in group),
                            'classes': '|'.join(classes),
                            'members': '|'.join(sorted(r['address'] for r in group)),
                            'dependencies': '', 'dependency_evidence': '', 'external_guest_dependencies': '',
                            'order_reason': f'Риск {risk}; листовые {leaf}/{len(group)}; строки hook-inventory {hooked}; '
                                            f'PC-класс {pc}/{len(group)}. При равенстве — имя класса/части. '
                                            'Связи вызовов справочные; T10 сначала проверяет ABI и чистоту каждого кандидата.',
                            'checks': checks})
    # Risk dominates; inside a risk tier, leaf fraction, existing port pain
    # (hook inventory) and PC availability define reproducible priority.
    batches.sort(key=lambda b: (b['risk'], -float(b['leaf_fraction']), -b['hooked']/b['functions'],
                                -b['pc_classes']/b['functions'], b['title']))
    byaddr = {r['address']: r for r in rows}
    for i, batch in enumerate(batches, 1):
        batch['order'], batch['batch'] = i, f'B{i:03}'
        for a in batch['members'].split('|'):
            byaddr[a]['batch'] = batch['batch']
    for batch in batches:
        edges, external = defaultdict(list), Counter()
        for a in batch['members'].split('|'):
            for p, target in features[int(a, 16)]['calls']:
                other = byaddr[hx(target)]
                if other['batch'] and other['batch'] != batch['batch']:
                    edges[other['batch']].append([a, hx(p), hx(target)])
                elif not other['batch']:
                    external[other['assignment']+':'+other['region']] += 1
        batch['dependencies'] = '|'.join(sorted(edges))
        batch['dependency_evidence'] = packed({d: {'call_sites': len(e), 'example': e[0]} for d, e in sorted(edges.items())})
        batch['external_guest_dependencies'] = packed(external)
    return batches, {}


def samples(rows):
    return {level: random.Random(SEED).sample([r['address'] for r in rows
                if is_new_level(r, level)], 20)
            for level in ('medium', 'low')}


def is_new_level(row, level):
    evidence = json.loads(row['evidence'])
    return (row['class_confidence'] == level and not evidence['t11_seed'] and
            not (level == 'low' and evidence.get('t11_previous_low_class') == row['class']))


def validate(rows, batches, image, features):
    assert [int(r['address'], 16) for r in rows] == image.funcs
    assert sum(r['bytes'] for r in rows) == image.end-image.funcs[0]
    members = [a for b in batches for a in b['members'].split('|')]
    assert len(members) == len(set(members))
    assert set(members) == {r['address'] for r in rows if r['batch']}
    assert all(r['class'] not in ('unknown', '<global>') for r in rows if r['batch'])
    assert not any(r['batch'] or r['evidence_kind'] == 'unique-field-signature' for r in rows if r.get('library'))
    byaddr = {r['address']: r for r in rows}
    for address in ('0x82574820', '0x82598098', '0x82598310', '0x825985D0'):
        assert byaddr[address]['region'] == 'middleware' and byaddr[address]['class'] == 'unknown'
    for r in rows:
        if r['evidence_kind'] == 'unique-field-signature' and r['class_confidence'] == 'medium':
            assert json.loads(r['evidence'])['signature_support']
    assert all(r['assignment'] == 'unassigned' for r in rows if r['class'] == 'unknown' and r['region'] in ('game', 'sexy'))
    assert sum(b['bytes'] for b in batches) == sum(r['bytes'] for r in rows if r['batch'])
    assert all(b['order_reason'] and 0 < b['functions'] <= 150 for b in batches)
    assert [b['order'] for b in batches] == list(range(1, len(batches)+1))
    seeds = [(r['address'], r['class'], r['class_confidence']) for r in rows if json.loads(r['evidence'])['t11_seed']]
    assert len(seeds) == 1358
    assert digest(json.dumps(seeds, separators=(',', ':')).encode()) == '7575535ffde9c2e94161cc3971a2ee23ce29c5b2d6094c21713beb74f135200a', 'T11 strong seed changed'
    assert all(f['reachable_bytes'] <= image.ends[a]-a for a, f in features.items())


def table(headers, values):
    safe = lambda s: str(s).replace('|', '\\|').replace('\n', ' ')
    return ['| '+' | '.join(headers)+' |', '| '+' | '.join('---' for _ in headers)+' |'] + [
        '| '+' | '.join(safe(v) for v in row)+' |' for row in values]


def review_context(image, inputs, level, offset):
    """Display raw Capstone evidence; does NOT score its own predictions."""
    sys.path.insert(0, str(ROOT/'tools/python'))
    from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN
    cs = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
    csv.field_size_limit(16*1024*1024)
    payload = json.loads(inputs.text(RESEARCH/'full-analysis.json'))
    assert payload['inputs']['.cache/pvz-image.bin'] == inputs.hashes['.cache/pvz-image.bin']
    rows = [{**r, 'evidence': packed(r['evidence'])} for r in payload['functions']]
    lookup = {r['address']: r for r in rows}
    print('SHA-256 inputs: '+packed(inputs.hashes))
    def instructions(address, size):
        for ins in cs.disasm(image.raw[address-BASE:address-BASE+size], address):
            print(f'{ins.address:08X} {ins.mnemonic:9} {ins.op_str}')
    for a in samples(rows)[level][offset:offset+5]:
        row, address = lookup[a], int(a, 16)
        ev = json.loads(row['evidence'])
        print('\nREVIEW '+level+' '+a+' '+row['class']+' '+row['evidence_kind'])
        instructions(address, min(64, image.ends[address]-address))
        if 'field_signature' in ev:
            sig = ev['field_signature']
            print('OFFSETS '+packed(sig['offsets']))
            seen = set()
            for e in sig['instructions']:
                if e['offset'] not in seen:
                    instructions(int(e['instruction'], 16), 4)
                    seen.add(e['offset'])
            for off, refs in sig['references'].items():
                print('REFERENCE '+off+' '+packed(refs[:1]))
                instructions(int(refs[0].get('instruction', refs[0].get('layout_instruction')), 16), 4)
        for e in ev.get('vtable_stores', []):
            p = int(e['instruction'], 16)
            print('STORE '+packed(e))
            instructions(p-12, 16)
        contexts = ev.get('typed_calls', ev.get('static_call_contexts', []))
        seen_callers = set()
        print('STATIC CONTEXTS '+str(len(contexts)))
        for e in contexts:
            if e['caller'] in seen_callers:
                continue
            seen_callers.add(e['caller'])
            print('CALLER '+packed(e))
            caller, p = int(e['caller'], 16), int(e['instruction'], 16)
            print('CALLER LABEL '+lookup[e['caller']]['class']+' '+lookup[e['caller']]['evidence_kind'])
            instructions(caller, min(32, image.ends[caller]-caller))
            instructions(max(caller, p-24), p-max(caller, p-24)+4)


def make_report(rows, batches, features, inputs, image, outputs, planning, manifest):
    confidence = Counter(r['class_confidence'] for r in rows)
    unassigned = [r for r in rows if r['assignment'] == 'unassigned']
    lines = ['# T11 — классы Xbox и план пакетов (пересчёт T11b)', '', '## 1. Итог', '',
             f'Карта содержит {len(rows)} функций: high {confidence["high"]}, medium {confidence["medium"]}, '
             f'low {confidence["low"]}, unknown {confidence["unknown"]}. План: {len(batches)} пакетов; '
             f'отдельный список unassigned: {len(unassigned)} функций. Поведение игры не менялось.', '',
             'Сильные назначения T11 сохранены: 1 358 пар адрес/класс/уровень; fingerprint '
             '`7575535ffde9c2e94161cc3971a2ee23ce29c5b2d6094c21713beb74f135200a`. '
             'Проверяется validate() независимо от выходного CSV.', '',
             '## 2. Изменённые файлы', '',
             '- `scripts/classify_xbox_functions.py` — PPC/RTTI, T04, замыкание, сверка имён, план и проверка.',
             '- `docs/native-port/data/xbox-function-classes.csv` — компактные восемь колонок, без JSON.',
             '- `docs/native-port/data/replacement-batches.csv` — непересекающиеся пакеты с причинами порядка.',
             '- `docs/native-port/reports/T11.md` — этот воспроизводимый отчёт.',
             '- `docs/native-port/reports/T11b.md` — ручной отчёт и новая выборка исполнителя.',
             '- `research/function-classes/full-analysis.json` — полные записи с доказательствами, вне git.',
             '- `research/function-classes/library-zones.json` — корни библиотек и границы, вне git.', '',
             '## 3. Как воспроизвести', '', '```powershell',
             'python -I scripts/classify_xbox_functions.py --self-test',
             'python -I scripts/classify_xbox_functions.py',
             'python -I scripts/classify_xbox_functions.py --check --sample',
             'python -I scripts/classify_xbox_functions.py --review-context medium --review-offset 0',
             '# Повторить --review-offset 5, 10, 15; затем те же четыре команды для low.',
             'python -I scripts/classify_xbox_functions.py --inspect 8212C540', '```', '',
             'Capstone: локальный tools/python, Android extractor: name_xbox_from_android.py. '
             '--check пересчитывает оба CSV, оба research JSON и отчёт, требует побайтного '
             'совпадения и ничего не пишет. Если research отсутствует, сначала нужен обычный запуск. '
             '--review-context читает полные доказательства из full-analysis.json.', '',
             '## 4. Результаты', '',
             '**Проверено запуском.** bytes — непересекающийся диапазон до следующего старта '
             '(включая padding/data); reachable_bytes — нижняя граница по достижимому CFG. '
             'Области заданы адресом с коррекцией встроенных библиотек; geographic_region '
             'в research сохраняет исходную область. semantic_area указан отдельно.', '']
    coverage = []
    for area, _, _ in REGIONS:
        group = [r for r in rows if r['region'] == area]
        size = sum(r['bytes'] for r in group)
        strong = [r for r in group if r['class'] not in ('unknown', '<global>') and r['class_confidence'] in ('high', 'medium')]
        covered = sum(r['bytes'] for r in strong)
        coverage.append([area, len(group), size, len(strong), covered, f'{100*covered/size:.2f}%'])
    lines += table(['Область', 'Функций', 'Байты', 'Класс H/M', 'Байты H/M', 'Охват'], coverage)
    nominal = [r for r in rows if r['geographic_region'] == 'sexy']
    nominal_covered = sum(r['bytes'] for r in nominal if r['class'] not in ('unknown', '<global>') and r['class_confidence'] in ('high', 'medium'))
    lines += ['', f'Для сравнения с прежним отчётом: H/M на исходном знаменателе sexy '
              f'{sum(r["bytes"] for r in nominal)} байт: {nominal_covered} байт '
              f'({100*nominal_covered/sum(r["bytes"] for r in nominal):.2f}%). '
              'Процент sexy в таблице использует уменьшенную область после выделения middleware.', '',
              '### Встроенные библиотеки', '',
              '**Подтверждено косвенно по Xbox.** Корни: принятые png-имена, диагностические строки, '
              'JPEG-таблица сообщений и его error-протокол ([this+0] → msg_code +14 → callback), '
              'zlib version 1.1.3 + размер z_stream 0x38 на границе вызова и copyright-строки. '
              'Замыкание: прямые calls, '
              'literal callback и readonly-таблицы, а также callers с тем же неизменным r3. '
              'Принятые классы/RTTI — барьеры. Соседство адресов не заполняет пропуски. '
              'Точное имя безымянного helper не утверждается. Библиотечные функции не участвуют '
              'в обучении/назначении сигнатур или распространении новых классов.', '']
    lines += table(['Семейство', 'Функций'], sorted(planning['libraries']['families'].items()))
    lines += ['', 'Непересекающиеся зоны, конец исключён; пути до корней и инструкции — в research:', '']
    lines += table(['Начало', 'Конец', 'Семейство', 'Функций'],
                   [[z['start'], z['end'], z['family'], z['functions']] for z in planning['libraries']['zones']])
    lines += ['', '### Формат результатов', '',
              'Компактный CSV: address, region, bytes, class, class_confidence, evidence_kind, '
              'key_instruction, batch. Полная запись ищется по address в functions массива '
              'full-analysis.json. key_instruction — PPC call/store/load либо вход функции '
              'для RTTI-якоря; RTTI-слоты находятся в JSON. Входные SHA и общий манифест '
              'вынесены в research JSON и этот отчёт, без повторения на каждой строке CSV.', '']
    lines += ['', 'Свободные функции `<global>` исключены из строгого охвата класса. '
              'При прежнем учёте T11-review (с `<global>`) game: '
              f'{sum(r["bytes"] for r in rows if r["region"] == "game" and r["class_confidence"] in ("high", "medium"))} '
              'байт; исходные 248 064 байта T11 включали 5 028 байт свободных функций.', '']
    lines += ['', 'Источники класса:', '']
    lines += table(['Признак', 'Функций'], sorted(Counter(r['evidence_kind'] for r in rows).items()))
    lines += ['', '### Метод и ограничения достоверности', '',
              '- **high / проверено Xbox:** прежние RTTI-vtable и принятые имена T01/T01b.',
              '- **medium / подтверждено косвенно:** сохранённые T11; новый класс по всем статическим '
              'call sites с одинаковым r3 (двое разных H/M callers), либо по проверенному полю T04; '
              'либо уникальная полная сигнатура из двух и более смещений с шириной доступа '
              'и отдельным H/M caller с разрешённым r3 того же класса (signature_support).',
              '- **low / гипотеза:** единственный identity-caller или цепочка, содержащая low. '
              'Неподтверждённая сигнатура также low. Low не превращается в medium за счёт '
              'количества своих потомков, циклы low не подтверждают себя.',
              '- **unknown:** конфликт либо недостаток данных. Непрояснённый caller блокирует '
              'назначение по вызовам. Саморекурсия не даёт второго caller.', '',
              'Типы указательных полей перечислены в FIELD_TYPES; смещения, форма массива и '
              'Xbox-доказательство загружаются из T04. Нет типа по одному лишь GuestPtr. '
              'Выражения this+N и secondary-vtable учитываются: для нового класса нужен нулевой '
              'итоговый adjustment, неоднозначный adjustment блокирует вывод. Поля отсчитываются '
              'от полного объекта. В research указаны call site, caller, r3-origin и цепочка загрузок полей.', '',
              'PPC: пересечение на слияниях, уничтожение volatile после обычных вызовов. '
              'Store-only save helpers GPR/FPR проверяются по каждому PPC-слову до blr, включая '
              'внутренние точки входа общей процедуры, и сохраняют r3. '
              'Нет типов возвращаемых значений, stack spills, динамических целей и восстановления '
              'jump tables. Возможна недооценка охвата. Сигнатуры уникальны в наблюдаемом корпусе, '
              'а не во всей возможной программе; это косвенный признак. Противоречащие им '
              'типизированные callers оставляют функцию unknown/conflict, и замыкание строится заново. '
              'Новые прямые записи vtable дают дополнительные Xbox-якоря без потери исходных T11.', '',
              f'Замыкание: `{packed(planning["classification"])}`.', '',
              '### Подтверждение методов', '',
              f'Поиск Android/PC: `{packed(planning["methods"])}`; новых имён: '
              f'{sum(r["method_confidence"] == "medium" and "method_confirmation" in json.loads(r["evidence"]) for r in rows)}. '
              'Класс фиксируется до поиска. Требуются два разных принятых именованных callee и '
              'две константы >255, уникальный кандидат того же класса; дубли Xbox отбрасываются. '
              'PC-поле — наличие квалифицированного определения метода; одинаковое имя не доказывает '
              'эквивалентность ABI/поведения.', '', '### Остаток игрового кода', '']
    residual = defaultdict(list)
    for r in rows:
        if r['region'] == 'game' and (r['class_confidence'] not in ('high', 'medium') or r['class'] == '<global>'):
            reason = ('global / free-functions-no-class' if r['class'] == '<global>' else
                      'unconfirmed-field-signature' if r['evidence_kind'] == 'unique-field-signature' else
                      json.loads(r['evidence']).get('unassigned_reason', 'low / only-one-or-low-callers'))
            residual[reason].append(r)
    lines += table(['Причина', 'Функций', 'Байты'], [(k, len(v), sum(r['bytes'] for r in v)) for k, v in sorted(residual.items())])
    lines += ['', '### План пакетов', '',
              'Пакет — класс либо подсистема из родственных малых классов одного риска. '
              'Крупные группы делятся на части <=150 функций с листовыми первыми; остатки '
              'подсистем меньше 20 сохраняются целиком и видны в таблице. '
              'Порядок: риск (0 обычный, 2 рендер/анимации, 3 звук/платформа), убывание доли '
              'листовых, доли функций с хуками T03, доли PC-класса, имя. Листовая не означает '
              'чистая: T10 отдельно проверяет запись памяти/ABI каждой замены. README задаёт '
              'проблемы в checks; hook-inventory — измеримый proxy затронутости порта, '
              'не утверждение, что каждый хук является открытой ошибкой. Вызовы справочные, '
              'циклы не ограничивают порядок гибридных замен.', '']
    lines += table(['№', 'Пакет', 'Класс / часть', 'Функций', 'Листовых', 'С хуком', 'PC класс', 'Риск'],
                   [[b['order'], b['batch'], b['title'], b['functions'], b['leaf_functions'], b['hooked'], b['pc_classes'], b['risk']] for b in batches])
    lines += ['', '### unassigned', '',
              'Полный отдельный список в full-analysis.json: `functions[].assignment=unassigned`; '
              'каждая строка содержит address, region, bytes и evidence_kind. '
              'В пакетах нет unknown и свободных функций без класса (<global>). Сводка:', '']
    lines += table(['Область', 'Функций', 'Байты'],
                   [(area, len(g := [r for r in unassigned if r['region'] == area]), sum(r['bytes'] for r in g)) for area in sorted({r['region'] for r in unassigned})])
    lines += ['', '## 5. Критерии приёмки', '',
              f'- Новая выборка 20 medium: seed {SEED}; протокол в T11b.md.',
              '- validate(): нет библиотек в пакетах/сигнатурах; medium-сигнатуры имеют signature_support.',
              '- Охват >=50% оценивается по таблице game выше; если меньше — частичная приёмка, остаток разобран по причинам.',
              '- Новые назначения содержат инструкции, callers либо сигнатуру с независимыми Xbox-примерами.',
              '- validate(): пакеты без unknown, без пересечений; у всех есть order_reason.',
              '- --check сверяет оба CSV, оба research JSON и отчёт побайтно; все 1 358 прежних high/medium сохранены.', '',
              '## 6. Не проверено и риски', '',
              'Полнота графа, ABI и эквивалентность будущих нативных замен не доказаны. '
              'Сигнатуры и контекст r3 могут относиться к free helpers или общей базовой '
              'реализации. Изолированные функции библиотек без roots/calls/callback-ссылок '
              'могут остаться неизвестными. Новая ручная выборка — в T11b.md. '
              'Эти таблицы требуют проверки перед нативной заменой. Неизвестные indirect-only цели '
              'нельзя строго отделить от мёртвого/экспортируемого кода: метка indirect-target-possible '
              'показывает лишь наличие адресных ссылок. Игра и сборка не запускались.', '',
              '## 7. Вопросы к владельцу', '',
              'Блокирующих решений нет. Принятие результата и выбор конкретной замены остаются '
              'за проверяющим. README, src, CMakeLists, generated и входные данные не менялись.', '',
              '## Выборка для проверки', '', f'Seed: {SEED}; random.sample отдельно по новым medium и low; адреса отсортированы. '
              'Из low исключены прежние low T11 с тем же классом (пересчитываются исходным однопроходным правилом).', '']
    lookup = {r['address']: r for r in rows}
    lines += table(['Уровень', 'Адрес', 'Класс', 'Признак'],
                   [['medium', a, lookup[a]['class'], lookup[a]['evidence_kind']] for a in samples(rows)['medium']])
    lines += ['', '## SHA-256 выходов', ''] + table(['Файл', 'Байты', 'SHA-256'], [(p.relative_to(ROOT).as_posix(), len(raw), digest(raw)) for p, raw in outputs.items()])
    lines += ['', f'## SHA-256 входов ({len(inputs.hashes)} файлов)', '', f'Манифест: `{manifest}`.', '']
    lines += table(['Вход', 'SHA-256'], sorted(inputs.hashes.items()))
    return '\n'.join(lines)+'\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--sample', action='store_true')
    parser.add_argument('--review-context', choices=('medium', 'low'), help='Show raw sample evidence from existing CSV; no regeneration')
    parser.add_argument('--review-offset', type=int, default=0, help='Sample offset, five entries per display')
    parser.add_argument('--inspect', type=lambda x: int(x, 16))
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    inputs = Inputs()
    inputs.read('scripts/classify_xbox_functions.py')
    # Provenance of the manual audit, not class-prediction inputs.
    for path in ('scripts/name_xbox_from_android.py', 'scripts/analyze_cross_platform.py',
                 'Chinese Android TV Image/lib/armeabi/libGameMain.so'):
        inputs.read(path)
    image = Image(inputs)
    if args.review_context:
        review_context(image, inputs, args.review_context, args.review_offset)
        return
    names = {int(r['xbox_address'], 16): r for r in inputs.csv('docs/native-port/data/xbox-names.csv')}
    assert set(names) <= image.fset
    for row in names.values():
        ev = json.loads(row['evidence'])
        assert ev['xbox_image_sha256'] == inputs.hashes['.cache/pvz-image.bin']
        assert ev['android_sha256'] == inputs.hashes['Chinese Android TV Image/lib/armeabi/libGameMain.so']
        assert ev['signals'] and any(s['kind'] != 'android-only' for s in ev['signals'])
    hooks = defaultdict(list)
    for row in inputs.csv('docs/native-port/data/hook-inventory.csv'):
        if row['kind'] == 'hook':
            address = row['guest_address'] or row['symbol'].removeprefix('sub_')
            if re.fullmatch(r'(?:0x)?[0-9A-Fa-f]{8}', address):
                hooks[int(address, 16)].append(row['file']+':'+row['line'])
    android = json.loads(inputs.text('research/cross-platform/android-symbols.json'))
    android_classes = {split_name(s['demangled'])[0] for s in android if s['type'] == 2 and s['section']}
    global_methods = sorted({split_name(r['name'])[1].split('(')[0] for r in names.values()
                             if split_name(r['name'])[0] in ('', 'Sexy')})
    pc_classes, pc_methods = pc_index(inputs, global_methods)
    print('Analyzing reachable PPC code...', flush=True)
    # Freeze the accepted T11 analysis, including its conservative call effects.
    seed_features = {a: seed_analyze(image, a) for a in image.funcs}
    seeds = classify(image, seed_features, names)
    previous_low_contexts = defaultdict(set)
    for a, seed in seeds.items():
        if seed['class'] in ('unknown', '<global>'):
            continue
        for _, target in seed_features[a]['this_calls']:
            if target != a:
                previous_low_contexts[target].add(seed['class'])
    previous_low = {a: next(iter(owners)) for a, owners in previous_low_contexts.items()
                    if len(owners) == 1 and seeds[a]['class'] == 'unknown' and region(a) in ('game', 'sexy')}
    # Save routines are shared assembly sequences: bl often enters their
    # INTERIOR (e.g. 825FC808), absent from pvz_funcs.h. Test all direct bl
    # targets, not just generated function starts.
    helper_targets = {branch(BASE+4*i, w) for i, w in enumerate(image.words)
                      if image.funcs[0] <= BASE+4*i < image.end and w >> 26 == 18 and w & 1}
    helpers = {a for a in helper_targets if save_helper(image, a)}
    features = {a: analyze(image, a, helpers) for a in image.funcs}
    layouts = inputs.csv('docs/native-port/data/xbox-layouts.csv')
    anchors = classify(image, features, names)
    for a, seed in seeds.items():
        if seed['class'] != 'unknown':
            anchors[a] = seed
    libraries, library_stats = library_regions(image, features, anchors, names)
    print('Embedded libraries: '+packed(library_stats['families']), flush=True)
    classified, classification_stats = extend_classes(image, features, anchors, layouts, seeds, libraries)
    classification_stats['new_vtable_store_anchors'] = sum(seeds[a]['class'] == 'unknown' and
                                                          anchors[a]['class'] != 'unknown' for a in image.funcs)
    for a, item in classified.items():
        if a in previous_low:
            item['evidence']['t11_previous_low_class'] = previous_low[a]
    print('Confirming methods against same-class Android bodies...', flush=True)
    confirmed, method_stats = confirm_methods(image, features, classified, names, android, pc_methods)
    if args.inspect is not None:
        a = args.inspect
        print('xbox_image_sha256='+inputs.hashes['.cache/pvz-image.bin'])
        print(packed(classified[a]))
        sys.path.insert(0, str(ROOT/'tools/python'))
        from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN
        cs = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
        for ins in cs.disasm(image.raw[a-BASE:image.ends[a]-BASE], a):
            print(f'{ins.address:08X} {ins.mnemonic:10} {ins.op_str}')
        return
    rows = []
    for a in image.funcs:
        item, named = classified[a], names.get(a) or confirmed.get(a)
        if a in confirmed:
            item['evidence']['method_confirmation'] = confirmed[a]['evidence']
        method = split_name(named['name'])[1] if named else ''
        key = method.split('(')[0]
        cls = item['class']
        rows.append({'address': hx(a), 'region': 'middleware' if a in libraries else region(a),
                     'geographic_region': region(a), 'library': libraries.get(a, {}).get('family', ''),
                     'bytes': image.ends[a]-a,
                     'reachable_bytes': features[a]['reachable_bytes'], 'class': cls,
                     'class_confidence': item['confidence'], 'name': named['name'] if named else '', 'method': method,
                     'method_confidence': named['confidence'] if named else '',
                     'evidence_kind': item['source'], 'evidence': packed(item['evidence']),
                     'pc_class_files': '|'.join(sorted(pc_classes.get(cls, []))),
                     'pc_method_files': '|'.join(sorted(pc_methods.get((cls, key), []))),
                     'android_class_present': int(cls in android_classes),
                     'port_hooks': '|'.join(sorted(hooks[a])), 'batch': ''})
    batches, planning = plan_batches(rows, features, inputs)
    planning.update(classification=classification_stats, methods=method_stats, libraries=library_stats)
    manifest = digest(packed(inputs.hashes).encode())
    for row in batches:
        row['input_manifest_sha256'] = manifest
    compact = []
    for row in rows:
        ev = json.loads(row['evidence'])
        witnesses = (ev.get('signature_support') or ev.get('typed_calls') or ev.get('vtable_stores') or
                     ev.get('field_signature', {}).get('instructions') or [])
        instruction = next((w['instruction'] for w in witnesses if 'instruction' in w), '')
        instruction = instruction or ev.get('library', {}).get('instruction', '')
        if not instruction and ev.get('vtable_hosts'):
            instruction = row['address']
        if not instruction and row['class'] != 'unknown':
            instruction = row['address']
        compact.append({**{key: row[key] for key in ('address', 'region', 'bytes', 'class', 'class_confidence', 'evidence_kind')},
                        'key_instruction': instruction, 'batch': row['batch']})
    payload = {'schema': 2, 'input_manifest_sha256': manifest, 'inputs': inputs.hashes,
               'functions': [{**r, 'evidence': json.loads(r['evidence'])} for r in rows]}
    outputs = {
        DATA/'xbox-function-classes.csv': csv_bytes(compact, list(compact[0])),
        DATA/'replacement-batches.csv': csv_bytes(batches, list(batches[0])),
        RESEARCH/'full-analysis.json': (packed(payload)+'\n').encode('utf-8'),
        RESEARCH/'library-zones.json': (packed({'input_manifest_sha256': manifest, 'inputs': inputs.hashes, **library_stats})+'\n').encode('utf-8'),
    }
    report = make_report(rows, batches, features, inputs, image, outputs, planning, manifest)
    outputs[ROOT/'docs/native-port/reports/T11.md'] = report.encode('utf-8')
    validate(rows, batches, image, features)
    if args.sample:
        print(packed(samples(rows)))
    for path, raw in outputs.items():
        if args.check:
            assert path.exists() and path.read_bytes() == raw, f'Output differs: {path}'
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
        print(f'{path.relative_to(ROOT).as_posix()}: {len(raw)} bytes sha256={digest(raw)}')
    print(packed({'functions': len(rows), 'class_confidence': dict(Counter(r['class_confidence'] for r in rows)),
                  'batches': len(batches), 'manifest': manifest}))


if __name__ == '__main__':
    main()
