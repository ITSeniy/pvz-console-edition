"""Name Xbox 360 PvZ functions from Android TV libGameMain.so symbols.

Inputs (read only): .cache/pvz-image.bin (Xbox image at 0x82000000),
generated/default/pvz_funcs.h (function starts), research/cross-platform/
android-symbols.json (from analyze_cross_platform.py) and the Android TV
libGameMain.so. Output: research/cross-platform/xbox-android-names.json.

Method:
1. Xbox classes, primary/secondary vtables and bases come from MSVC RTTI
   (CompleteObjectLocator before each vtable). Android vtables and bases come
   from Itanium vtables/typeinfo resolved through .rel.dyn.
2. Android slots are put into MSVC order: the D1+D0 destructor pair becomes
   one scalar deleting destructor slot, and overloads introduced by a class are
   grouped at the first one in reverse declaration order. The Xbox build is an
   older framework (Widget has 71 slots against 121), so slots introduced by
   each class are aligned with Needleman-Wunsch. A slot pair scores by
   function shape (empty / return-constant / PPC vs Thumb size) summed over
   every distinct function pair that occupies the slot in the class subtree.
3. Unique nontrivial string references constrain the alignment. Size/shape
   alone always remains low. High requires independent vtable + unique-string
   evidence, or unique strings + an ordered sequence of at least two distinct
   string-identified callees. Uncorroborated call-gap propagation is disabled.
4. T01b confirms unambiguous vtable candidates once against frozen T01 seeds:
   unique semantic integers, ordered named callees, or anchored caller ordinals.
   Reachable-code extraction excludes inline data and stops at indirect jumps.
Hand-written `sub_X // Class::Method` comments in src/ are used only to check
the result. Addresses belong to this image only; nothing is run or patched.
"""
from pathlib import Path
from collections import Counter, defaultdict
from functools import lru_cache
import math
import argparse
import csv
import json
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
# Isolated Python (-I) omits the script directory from sys.path.
sys.path.insert(0, str(ROOT / 'scripts'))
from analyze_cross_platform import elf32, save_json, sha

OUT = ROOT / 'research/cross-platform'
sys.path.insert(0, str(ROOT / 'tools/python'))
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN  # noqa: E402

BASE = 0x82000000
SIZE_SLOPE, SIZE_BIAS = 1.5, 24   # PPC bytes ~= 1.5 * Thumb bytes + 24
GAP = -0.6
FLIP = 0.5   # penalty when only one platform overrides a slot in a subclass
CONF_RANK = {'low': 0, 'medium': 1, 'high': 2}


def method_key(name):
    head = name.split('(')[0]
    return head.rsplit('::', 1)[-1]


RUNTIME = ('__aeabi', '__gnu', '__cxa', '_Unwind')  # Android-only ABI/EH helpers


def collapse(calls):
    """Drop immediate repeats (unrolled or EH-duplicated calls differ by platform)."""
    return [c for i, c in enumerate(calls) if not i or calls[i - 1] != c]


def printable(raw):
    """ASCII text of at least 4 characters with a letter, else None."""
    if len(raw) < 4 or not all(32 <= c < 127 or c in (9, 10, 13) for c in raw):
        return None
    text = raw.decode('ascii')
    return text if re.search('[A-Za-z]', text) else None


def split_name(name):
    """'A::B::f(int) const' -> ('A::B', 'f(int) const')."""
    depth, cut = 0, -1
    head = name.split('(')[0]
    for i, c in enumerate(head):
        depth += c == '<'
        depth -= c == '>'
        if depth == 0 and head.startswith('::', i):
            cut = i
    return (name[:cut], name[cut + 2:]) if cut >= 0 else ('', name)


class Xbox:
    def __init__(self):
        self.data = (ROOT / '.cache/pvz-image.bin').read_bytes()
        n = len(self.data) // 4
        self.words = struct.unpack('>%dI' % n, self.data[:n * 4])
        self.refs = defaultdict(list)
        end = BASE + len(self.data)
        for i, v in enumerate(self.words):
            if BASE <= v < end:
                self.refs[v].append(BASE + 4 * i)
        header = ROOT / 'generated/default/pvz_funcs.h'
        self.funcs = sorted({int(a, 16) for a in re.findall(r'sub_([0-9A-Fa-f]{8})', header.read_text())})
        self.fset = set(self.funcs)
        self.fsize = {a: b - a for a, b in zip(self.funcs, self.funcs[1:])}
        self.types = {BASE + m.start() - 8: self.class_name(m.group().decode('ascii'))
                      for m in re.finditer(rb'\.\?AV[^\x00]{1,150}(?=\x00)', self.data)}
        self.vtables = {}   # (class, offset) -> (address, slots)
        self.bases = {}     # class -> [(name, contained, mdisp)]
        for td, name in self.types.items():
            for ref in self.refs[td]:
                col = ref - 12
                if self.rd(col) or self.rd(col + 8) > 0x1000 or self.rd(col + 4) > 0x10000:
                    continue
                hier = self.rd(col + 16)
                if not 1 <= self.rd(hier + 8) <= 64:
                    continue
                self.bases.setdefault(name, self.hierarchy(hier))
                for vref in self.refs[col]:
                    slots, a = [], vref + 4
                    while self.rd(a) in self.fset:
                        slots.append(self.rd(a))
                        a += 4
                    self.vtables[(name, self.rd(col + 4))] = (vref + 4, slots)

    @staticmethod
    def class_name(rtti):
        # Template encodings require a real MSVC demangler. Keep them opaque,
        # explicitly excluded from cross-platform class matching (not gibberish).
        if '?$' in rtti:
            return 'excluded-msvc-template:' + rtti
        return '::'.join(reversed(rtti.removeprefix('.?AV').removesuffix('@@').split('@')))

    def rd(self, a):
        return self.words[(a - BASE) // 4] if BASE <= a < BASE + 4 * len(self.words) else 0

    def hierarchy(self, hier):
        out, arr = [], self.rd(hier + 12)
        for i in range(self.rd(hier + 8)):
            b = self.rd(arr + 4 * i)
            mdisp = struct.unpack('>i', struct.pack('>I', self.rd(b + 8)))[0]
            out.append((self.types.get(self.rd(b), '?'), self.rd(b + 4), mdisp))
        return out

    def parent(self, cls):
        bases = self.bases.get(cls, [])
        if len(bases) > 1 and bases[1][2] == 0 and (bases[1][0], 0) in self.vtables:
            return bases[1][0]
        return None

    def secondary_bases(self, cls, offset):
        return {name for name, _, mdisp in self.bases.get(cls, [])[1:] if mdisp == offset}

    def kind(self, a):
        w = self.rd(a)
        if w == 0x4E800020:
            return 'ret'
        if w >> 16 == 0x3860 and self.rd(a + 4) == 0x4E800020:
            return 'ret%d' % (w & 0xFFFF)
        return ''

    def size(self, a):
        return self.fsize.get(a, 0)

    def deleting_dtor(self, a):
        """MSVC scalar deleting destructor: a call and a `flags & 1` test (delete)."""
        size = self.size(a)
        if not size or size > 192:
            return False
        code = [self.rd(p) for p in range(a, a + size, 4)]
        test = any((w >> 26 == 21 and (w >> 1) & 0x7FFF == 0x3FF) or
                   (w >> 26 == 28 and w & 0xFFFF == 1) for w in code)
        return test and len(self.calls(a)) >= 1

    def string_at(self, addr):
        off = addr - BASE
        if not 0 < off < len(self.data) or self.data[off - 1] not in b'\0':
            return None
        return printable(self.data[off:off + 256].split(b'\0')[0])

    def strings(self, a):
        """Strings addressed by lis/addi (or lis/ori) pairs inside the function."""
        hi, out = {}, []
        for p in range(a, a + self.size(a), 4):
            w = self.rd(p)
            op, rd, ra, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
            # A straight-line reaching definition, never a stale register across
            # a branch/call. Unknown X-form writes conservatively kill all facts.
            previous = dict(hi)
            if op in (16, 18, 19, 31):
                hi.clear()
            elif op in (7, 8, 12, 13, 14, 15, 32, 33, 34, 35, 40, 41, 42, 43, 46, 58):
                hi.pop(rd, None)
            elif op in (20, 21, 23, 24, 25, 26, 27, 28, 29, 37, 39, 45):
                hi.pop(ra, None)
            if op == 15 and ra == 0:
                hi[rd] = imm << 16
            elif op == 14 and ra != 0 and ra in previous:
                lo = imm - 0x10000 if op == 14 and imm & 0x8000 else imm
                text = self.string_at((previous[ra] + lo) & 0xFFFFFFFF)
                if text:
                    out.append(text)
            elif op == 24 and rd in previous:
                text = self.string_at(previous[rd] | imm)
                if text:
                    out.append(text)
        return out

    @lru_cache(maxsize=None)
    def calls(self, a):
        out = []
        for p in range(a, a + self.size(a), 4):
            w = self.rd(p)
            if w >> 26 == 18 and w & 3 == 1:
                d = w & 0x03FFFFFC
                if d & 0x02000000:
                    d -= 0x04000000
                if p + d in self.fset:
                    out.append(p + d)
        return out


class Android:
    def __init__(self, symbols_json):
        path = ROOT / 'Chinese Android TV Image/lib/armeabi/libGameMain.so'
        self.data, self.sections, syms = elf32(path)
        if len(syms) != len(symbols_json) or any(
                any(raw[k] != enriched[k] for k in ('name', 'value', 'size', 'type', 'binding', 'section'))
                for raw, enriched in zip(syms, symbols_json)):
            raise ValueError('android-symbols.json does not match libGameMain.so')
        self.syms = symbols_json
        self.rel, self.plt = {}, {}
        plt = next(s for s in self.sections if s['name'] == '.plt')
        for sec in self.sections:
            if sec['type'] != 9:
                continue
            for k, i in enumerate(range(0, sec['size'], 8)):
                off, info = struct.unpack_from('<II', self.data, sec['offset'] + i)
                self.rel[off] = (info & 255, info >> 8)
                if sec['name'] == '.rel.plt':
                    self.plt[plt['address'] + 20 + 12 * k] = symbols_json[info >> 8]['demangled']
        self.by_addr, self.size = defaultdict(list), {}
        self.by_name = {}
        for s in symbols_json:
            if s['section'] and s['type'] == 2 and s['value']:
                a = s['value'] & ~1
                self.by_addr[a].append(s['demangled'])
                self.size[a] = s['size']
                self.by_name.setdefault(s['demangled'], a)
        self.pure_addr = self.by_name.get('__cxa_pure_virtual')
        self.typeinfo = {s['value']: s['demangled'][13:] for s in symbols_json
                         if s['demangled'].startswith('typeinfo for ') and s['section']}
        self.vt_symbol = {s['demangled'][11:]: s for s in symbols_json
                          if s['demangled'].startswith('vtable for ') and s['section']}
        abi = {s['value'] + 8: s['demangled'] for s in symbols_json
               if s['demangled'].startswith('vtable for __cxxabiv1::')}
        self.ti_kind = {v: k.rsplit('::', 1)[-1] for v, k in abi.items()}
        self.vtables = {c: self.groups(c) for c in self.vt_symbol}
        self.text = next(s for s in self.sections if s['name'] == '.text')
        self.rodata = next(s for s in self.sections if s['name'] == '.rodata')
        self.cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB)
        self.cs.detail = True

    def off(self, a):
        s = next(s for s in self.sections if s['type'] != 8 and s['address'] and
                 s['address'] <= a < s['address'] + s['size'])
        return s['offset'] + a - s['address']

    def rd(self, a):
        r = self.rel.get(a)
        if r and r[0] == 2:
            return self.syms[r[1]]['value'], self.syms[r[1]]['demangled']
        return struct.unpack_from('<I', self.data, self.off(a))[0], None

    def groups(self, cls):
        """{offset: [slot pointer or 'pure']} for each vtable group."""
        v = self.vt_symbol[cls]
        words = [self.rd(v['value'] + k) for k in range(0, v['size'], 4)]
        relocated = [(v['value'] + k) in self.rel for k in range(0, v['size'], 4)]
        groups, i = {}, 0
        while i < len(words):
            # header: [virtual-base offsets...] offset-to-top, typeinfo
            while i + 1 < len(words) and not relocated[i + 1]:
                i += 1
            if i + 1 >= len(words):
                break
            top = struct.unpack('<i', struct.pack('<I', words[i][0]))[0]
            i += 2
            slots = []
            while i < len(words) and relocated[i]:
                value, sym = words[i]
                pure = sym == '__cxa_pure_virtual' or not value or value & ~1 == self.pure_addr
                slots.append('pure' if pure else value & ~1)
                i += 1
            groups.setdefault(-top, slots)
        return groups

    def bases(self, cls):
        """[(base name, offset)] direct bases from typeinfo."""
        ti = next((a for a, n in self.typeinfo.items() if n == cls), None)
        if ti is None:
            return []
        kind = self.ti_kind.get(self.rd(ti)[0], '')
        if kind == '__si_class_type_info':
            return [(self.typeinfo.get(self.rd(ti + 8)[0], '?'), 0)]
        if kind == '__vmi_class_type_info':
            count = self.rd(ti + 12)[0]
            out = []
            for k in range(count):
                base = self.typeinfo.get(self.rd(ti + 16 + 8 * k)[0], '?')
                flags = struct.unpack('<i', struct.pack('<I', self.rd(ti + 20 + 8 * k)[0]))[0]
                if not flags & 1:
                    out.append((base, flags >> 8))
            return out
        return []

    def all_bases(self, cls, offset=0, seen=None):
        out = []
        for base, off in self.bases(cls):
            out.append((base, offset + off))
            out.extend(self.all_bases(base, offset + off))
        return out

    def parent(self, cls):
        b = self.bases(cls)
        return b[0][0] if b and b[0][1] == 0 and b[0][0] in self.vtables else None

    def name(self, a):
        if a == 'pure':
            return None
        if a in self.plt:
            return self.plt[a]
        names = self.by_addr.get(a)
        return names[0] if names else None

    def kind(self, a):
        if a == 'pure':
            return 'pure'
        h0, h1 = struct.unpack_from('<HH', self.data, self.off(a))
        if h0 == 0x4770:
            return 'ret'
        if h0 >> 8 == 0x20 and h1 == 0x4770:
            return 'ret%d' % (h0 & 0xFF)
        return ''

    def strings(self, a):
        """Strings addressed by `ldr rX, [pc, #n]` + `add rX, pc` (PIC) inside the function."""
        size = self.size.get(a, 0)
        lits, out = {}, []
        for ins in self.cs.disasm(self.data[self.off(a):self.off(a) + size], a):
            previous = dict(lits)
            _, writes = ins.regs_access()
            for reg in writes:
                lits.pop(ins.reg_name(reg), None)
            if ins.mnemonic.startswith(('b', 'cb')) or ins.mnemonic in ('pop', 'tbb', 'tbh'):
                lits.clear()
            if ins.mnemonic in ('ldr', 'ldr.w') and '[pc, #' in ins.op_str:
                lit = ((ins.address + 4) & ~3) + int(ins.op_str.split('#')[1].rstrip(']'), 0)
                if self.text['address'] <= lit < self.text['address'] + self.text['size']:
                    lits[ins.op_str.split(',')[0]] = struct.unpack_from('<I', self.data, self.off(lit))[0]
            elif ins.mnemonic == 'add' and ins.op_str.endswith(', pc'):
                reg = ins.op_str.split(',')[0]
                if reg in previous:
                    t = (previous[reg] + ins.address + 4) & 0xFFFFFFFF
                    if self.rodata['address'] < t < self.rodata['address'] + self.rodata['size'] and                             self.data[self.off(t) - 1] == 0:
                        text = printable(self.data[self.off(t):self.off(t) + 256].split(b'\0')[0])
                        if text:
                            out.append(text)
        return out

    @lru_cache(maxsize=None)
    def calls(self, a):
        size = self.size.get(a, 0)
        if not size:
            return []
        out = []
        code = self.data[self.off(a):self.off(a) + size]
        for ins in self.cs.disasm(code, a):
            if ins.mnemonic in ('bl', 'blx') and ins.op_str.startswith('#'):
                target = int(ins.op_str[1:], 0) & ~1
                if target != a:
                    out.append(target)
        return out


class Namer:
    def __init__(self):
        self.x = Xbox()
        symbols = __import__('json').loads((OUT / 'android-symbols.json').read_text(encoding='utf-8'))
        self.a = Android(symbols)
        self.classes = sorted(c for c in {c for c, _ in self.x.vtables} & set(self.a.vtables)
                              if not c.startswith(('std::', 'excluded-msvc-template:')))
        self.x_children = defaultdict(list)
        self.a_children = defaultdict(list)
        for c, o in self.x.vtables:
            if o == 0 and self.x.parent(c):
                self.x_children[self.x.parent(c)].append(c)
        for c in self.a.vtables:
            if self.a.parent(c):
                self.a_children[self.a.parent(c)].append(c)
        self.layout = {}
        self.colscore = {}
        # MSVC _purecall: the non-trivial body used at the most distinct slot indices.
        spread = defaultdict(set)
        for (c, o), (_, slots) in self.x.vtables.items():
            for i, x in enumerate(slots):
                if not self.x.kind(x):
                    spread[x].add((c, o, i))
        self.purecall = max(spread, key=lambda x: len({i for _, _, i in spread[x]}))
        self.sim_cache = {}
        self.columns = {}   # class -> {xbox slot: android normalized slot}
        self.evidence = defaultdict(list)   # xbox addr -> [dict]
        self.class_rows = []
        self.rejected_string_pairs = []

    # ---- Android slots in MSVC order ----
    def slot_key(self, cls, index):
        """Method name of raw Android primary slot `index`, looking in subclasses for pure slots."""
        stack = [cls]
        while stack:
            c = stack.pop()
            slots = self.a.vtables[c].get(0, [])
            if index < len(slots):
                name = self.a.name(slots[index])
                if name:
                    return method_key(name)
            stack.extend(self.a_children.get(c, []))
        return '?%d' % index

    def msvc_layout(self, cls):
        """List of raw-slot index tuples in MSVC order for the Android primary vtable."""
        if cls in self.layout:
            return self.layout[cls]
        parent = self.a.parent(cls)
        result = list(self.msvc_layout(parent)) if parent else []
        used = sum(len(e) for e in result)
        raw = self.a.vtables[cls].get(0, [])
        keys = [self.slot_key(cls, i) for i in range(used, len(raw))]
        entries, i = [], 0
        while i < len(keys):
            if keys[i].startswith('~') and i + 1 < len(keys) and keys[i + 1].startswith('~'):
                entries.append((keys[i], (used + i, used + i + 1)))
                i += 2
            else:
                entries.append((keys[i], (used + i,)))
                i += 1
        order, groups = [], defaultdict(list)
        for key, idx in entries:
            if key not in groups:
                order.append(key)
            groups[key].append(idx)
        for key in order:
            result.extend(reversed(groups[key]))
        self.layout[cls] = result
        return result

    def a_slots(self, cls, offset=0, layout_cls=None):
        raw = self.a.vtables[cls].get(offset, [])
        return [tuple(raw[i] for i in idx if i < len(raw)) for idx in self.msvc_layout(layout_cls or cls)]

    # ---- similarity ----
    def sim(self, x, a_entry):
        key = (x, a_entry)
        if key in self.sim_cache:
            return self.sim_cache[key]
        a = a_entry[-1]
        ka, kx = self.a.kind(a), self.x.kind(x)
        xs, as_ = self.x.size(x), self.a.size.get(a, 0)
        if ka == 'pure':
            s = 2.0 if x == self.purecall else 0.0
        elif x == self.purecall:
            s = -1.5
        elif len(a_entry) == 2 and method_key(self.a.name(a) or '').startswith('~'):
            s = 3.0 if self.x.deleting_dtor(x) else -1.5
        elif ka.startswith('ret') or kx.startswith('ret'):
            if ka == kx:
                s = 3.0 if ka != 'ret' else 2.0
            elif ka.startswith('ret') and kx.startswith('ret'):
                s = -1.0
            elif xs <= 16 or as_ <= 6:
                s = -0.5  # empty body on one side, forwarding stub on the other
            else:
                s = -2.0
        elif xs <= 16 and as_ <= 6:
            s = 1.0   # both are forwarding stubs
        elif not xs or not as_:
            s = 0.0
        else:
            r = abs(math.log(xs / (SIZE_SLOPE * as_ + SIZE_BIAS))) / math.log(2)
            s = max(-1.5, 1.5 - 2.0 * r)
            cx, ca = len(self.x.calls(x)), len(self.a.calls(a))
            if cx == ca:
                s += 1.0 if cx else 0.3
            elif abs(cx - ca) == 1:
                s += 0.3
            elif max(cx, ca) >= 2 * min(cx, ca) + 2:
                s -= 0.7
        self.sim_cache[key] = s
        return s

    def subtree_pairs(self, cls):
        out, stack = [], [cls]
        while stack:
            c = stack.pop()
            if c in self.a.vtables and (c, 0) in self.x.vtables:
                out.append(c)
            stack.extend(k for k in self.x_children.get(c, []) if k in self.a.vtables)
        return out

    def column_scores(self, cls, xs, as_):
        tables = []
        for c in self.subtree_pairs(cls):
            xp, ap = self.x.parent(c), self.a.parent(c)
            tables.append((self.x.vtables[(c, 0)][1], self.a_slots(c),
                           self.x.vtables[(xp, 0)][1] if c != cls and xp else None,
                           self.a_slots(ap) if c != cls and ap else None))
        score = {}
        for i in xs:
            for j in as_:
                seen, flips = set(), 0
                for xt, at, xpt, apt in tables:
                    if i >= len(xt) or j >= len(at) or not at[j]:
                        continue
                    seen.add((xt[i], at[j]))
                    if xpt is not None and apt is not None:
                        # a real column is overridden by the same subclasses on both sides
                        x_over = i >= len(xpt) or xpt[i] != xt[i]
                        a_over = j >= len(apt) or apt[j] != at[j]
                        flips += x_over != a_over
                semantic = 0
                for x, ae in sorted(seen, key=repr):
                    expected = self.string_pairs.get(x)
                    if expected and ae[-1] != 'pure':
                        semantic += 12 if int(expected['android'], 16) == ae[-1] else -12
                score[(i, j)] = math.fsum(self.sim(*p) for p in sorted(seen, key=repr)) - FLIP * flips + semantic
        return score

    @staticmethod
    def align(xs, as_, score):
        n, m = len(xs), len(as_)
        dp = [[0.0] * (m + 1) for _ in range(n + 1)]
        bt = [[0] * (m + 1) for _ in range(n + 1)]
        for i in range(1, n + 1):
            dp[i][0], bt[i][0] = dp[i - 1][0] + GAP, 1
        for j in range(1, m + 1):
            dp[0][j], bt[0][j] = dp[0][j - 1] + GAP, 2
        for i in range(1, n + 1):
            for j in range(1, m + 1):
                best, move = dp[i - 1][j - 1] + score[(xs[i - 1], as_[j - 1])], 0
                if dp[i - 1][j] + GAP > best:
                    best, move = dp[i - 1][j] + GAP, 1
                if dp[i][j - 1] + GAP > best:
                    best, move = dp[i][j - 1] + GAP, 2
                dp[i][j], bt[i][j] = best, move
        pairs, i, j = [], n, m
        while i or j:
            move = bt[i][j]
            if move == 0:
                pairs.append((xs[i - 1], as_[j - 1]))
                i, j = i - 1, j - 1
            elif move == 1:
                i -= 1
            else:
                j -= 1
        return pairs[::-1]

    def common_base(self, cls):
        """Nearest Xbox primary ancestor that is also an Android primary ancestor."""
        chain, c = set(), self.a.parent(cls)
        while c:
            chain.add(c)
            c = self.a.parent(c)
        c = self.x.parent(cls)
        while c and not (c in chain and c in self.a.vtables):
            c = self.x.parent(c)
        return c

    def align_class(self, cls):
        if cls in self.columns:
            return self.columns[cls]
        xslots = self.x.vtables[(cls, 0)][1]
        aslots = self.a_slots(cls)
        base = self.common_base(cls)
        mapping, scores = {}, {}
        if base:
            mapping = dict(self.align_class(base))
            scores = dict(self.colscore[base])
            xs = range(len(self.x.vtables[(base, 0)][1]), len(xslots))
            as_ = range(len(self.a_slots(base)), len(aslots))
        else:
            xs, as_ = range(len(xslots)), range(len(aslots))
        xs, as_ = list(xs), list(as_)
        score = self.column_scores(cls, xs, as_)
        new = self.align(xs, as_, score)
        for i, j in new:
            mapping[i] = j
            scores[i] = score[(i, j)]
        self.columns[cls] = mapping
        self.colscore[cls] = scores
        self.class_rows.append({'class': cls, 'xbox_parent': self.x.parent(cls),
                                'android_parent': self.a.parent(cls), 'aligned_from': base,
                                'xbox_new_slots': len(xs), 'android_new_slots': len(as_),
                                'matched_new_slots': len(new)})
        return mapping

    def confidence(self, score):
        # Size, constant-return bodies and override counts propose candidates;
        # none establishes semantic identity. Corroboration happens separately.
        return 'low'

    def add(self, xaddr, name, method, conf, android, context, note=None):
        row = {'name': name, 'method': method, 'confidence': conf,
               'android': hex(android) if isinstance(android, int) else android, 'context': context}
        if note:
            row['note'] = note
        self.evidence[xaddr].append(row)

    def name_vtables(self):
        self.colscore = {}
        for cls in self.classes:
            if (cls, 0) in self.x.vtables:
                self.align_class(cls)
        for cls in self.classes:
            if (cls, 0) not in self.x.vtables:
                continue
            xslots = self.x.vtables[(cls, 0)][1]
            aslots = self.a_slots(cls)
            xp = self.x.parent(cls)
            ap = self.a.parent(cls)
            px = self.x.vtables[(xp, 0)][1] if xp else []
            pa = self.a_slots(ap) if ap else []
            for i, j in self.columns[cls].items():
                x, a = xslots[i], aslots[j]
                if not a or a[-1] == 'pure':
                    continue
                x_over = not (i < len(px) and px[i] == x)
                a_over = not (j < len(pa) and pa[j] == a)
                if not x_over:
                    continue
                conf = self.confidence(self.colscore[cls].get(i, 0.0))
                name = self.a.name(a[-1]) or '?'
                note = 'MSVC scalar deleting destructor (Itanium D1+D0)' if len(a) == 2 else None
                if a_over:
                    self.add(x, name, 'vtable', conf, a[-1], '%s[%d]' % (cls, i), note)
                else:
                    _, method = split_name(name)
                    conf = 'medium' if conf == 'high' else 'low'
                    self.add(x, '%s::%s' % (cls, method), 'vtable-synth', conf, a[-1], '%s[%d]' % (cls, i),
                             'Xbox overrides, Android inherits %s' % name)
            # secondary vtables: same layout as the base's primary vtable
            abases = self.a.all_bases(cls)
            for (c, off), (vt, sx) in self.x.vtables.items():
                if c != cls or off == 0:
                    continue
                for base in sorted(self.x.secondary_bases(cls, off)):
                    if base not in self.columns:
                        continue
                    aoff = next((o for b, o in abases if b == base and o), None)
                    if aoff is None or aoff not in self.a.vtables[cls]:
                        continue
                    sa = self.a_slots(cls, aoff, base)
                    bx = self.x.vtables[(base, 0)][1]
                    for i, j in self.columns[base].items():
                        if i >= len(sx) or j >= len(sa) or not sa[j] or sa[j][-1] == 'pure':
                            continue
                        x = sx[i]
                        if i < len(bx) and bx[i] == x:
                            continue
                        conf = self.confidence(self.colscore[base].get(i, 0.0))
                        name = self.a.name(sa[j][-1]) or '?'
                        self.add(x, name, 'vtable', conf, sa[j][-1], '%s@%d as %s[%d]' % (cls, off, base, i))
                    break

    # ---- resolve evidence to one name per Xbox function ----
    def resolve(self, evidence):
        names = Counter(e['name'] for e in evidence)
        best = max(evidence, key=lambda e: (CONF_RANK[e['confidence']], names[e['name']]))
        row = {'name': best['name'], 'method': best['method'], 'confidence': best['confidence'],
               'android': best['android']}
        if 'note' in best:
            row['note'] = best['note']
        if len(names) > 1:
            row['aliases'] = sorted(names)
            row['confidence'] = 'low'
            row['note'] = 'several Android functions share this Xbox body (folded or conflicting)'
        row['contexts'] = sorted({e['context'] for e in evidence})
        return row

    # ---- string references ----
    def index_strings(self):
        self.x_strings = {f: set(self.x.strings(f)) for f in self.x.funcs}
        self.a_strings = {a: set(self.a.strings(a)) for a, size in self.a.size.items() if size}
        self.x_users, self.a_users = defaultdict(set), defaultdict(set)
        for f, texts in self.x_strings.items():
            for t in texts:
                self.x_users[t].add(f)
        for a, texts in self.a_strings.items():
            for t in texts:
                self.a_users[t].add(a)

    def name_by_strings(self):
        """Pair functions through strings that each platform references from one function only."""
        votes = defaultdict(Counter)
        for text, xs in sorted(self.x_users.items()):
            # Short tokens (NULL, File, TITLE...) are accidental cross-platform
            # singletons and have already produced demonstrably false names.
            if len(text.strip()) < 8:
                continue
            if len(xs) == 1 and len(self.a_users.get(text, ())) == 1:
                votes[next(iter(xs))][next(iter(self.a_users[text]))] += 1
        claimed = Counter(next(iter(v)) for v in votes.values() if len(v) == 1)
        out = {}
        for x, cands in votes.items():
            if len(cands) != 1:
                continue
            a, count = next(iter(cands.items()))
            name = self.a.name(a)
            if not name or claimed[a] != 1:
                continue
            # Shared file signatures do not identify functions across unrelated
            # audio backends (Xbox FMOD vs Android DUMB). Retain the rejected
            # evidence, rather than exporting an Android library symbol as Xbox.
            if name.startswith('dumb_') and any('FMOD' in t for t in self.x_strings[x]):
                self.rejected_string_pairs.append({'xbox': hex(x), 'android': hex(a), 'name': name,
                    'reason': 'different audio backends: Xbox self-identifies as FMOD, Android symbol belongs to DUMB',
                    'xbox_strings': sorted(self.x_strings[x])})
                continue
            out[x] = {'name': name, 'method': 'strings', 'confidence': 'medium',
                      'android': hex(a), 'contexts': sorted(self.x_strings[x] & self.a_strings[a]),
                      'evidence': [{'kind': 'unique-strings', 'strings': sorted(
                          t for t in self.x_strings[x] & self.a_strings[a]
                          if len(t.strip()) >= 8 and len(self.x_users[t]) == len(self.a_users[t]) == 1)}]}
        return out

    def corroborate(self, named):
        """Frozen string seeds + vtable agreement / ordered distinct direct callees.

        The golden set and source comments are deliberately unavailable here.
        No recursive promotion: a mistaken candidate cannot amplify itself.
        """
        seeds = {x: int(r['android'], 16) for x, r in self.string_pairs.items()}
        for x, row in sorted(named.items()):
            evidence = row.setdefault('evidence', [])
            if row['method'].startswith('vtable'):
                evidence.append({'kind': 'vtable-candidate', 'contexts': row['contexts']})
            if row.get('aliases'):
                continue
            a = int(row['android'], 16)
            string_row = self.string_pairs.get(x)
            string_support = bool(string_row and int(string_row['android'], 16) == a)
            if string_support and row['method'] != 'strings':
                evidence.extend(string_row['evidence'])
            # Only independently string-identified callees, not predictions
            # derived from this vtable alignment, may corroborate it.
            xc = list(dict.fromkeys(seeds[t] for t in self.x.calls(x) if t in seeds and t != x))
            ac = list(dict.fromkeys(t for t in self.a.calls(a) if t in set(xc)))
            calls_support = len(xc) >= 2 and xc == ac
            if calls_support:
                evidence.append({'kind': 'ordered-string-seed-callees', 'android': list(map(hex, xc))})
            # A string that *caused* a slot to align is not a second signal.
            # Require agreement with the alignment computed without strings.
            independent = self.independent_vtables.get(x)
            vt_support = bool(independent and independent['method'] == 'vtable'
                              and not independent.get('aliases') and independent['name'] == row['name'])
            if vt_support:
                evidence.append({'kind': 'vtable-without-string-scoring', 'contexts': independent['contexts']})
            if self.string_check(x, row['name']) == 'contradicted':
                row['confidence'] = 'low'
                evidence.append({'kind': 'disjoint-string-sets', 'xbox': sorted(self.x_strings[x]),
                                 'android': sorted(self.a_strings.get(a, set()))})
            elif string_support and (vt_support or calls_support):
                row['confidence'] = 'high'
            elif string_support or (vt_support and calls_support):
                row['confidence'] = 'medium'
            else:
                row['confidence'] = 'low'

    def string_check(self, x, name):
        """'supported' / 'contradicted' / None by overlap of referenced strings."""
        a = self.a.by_name.get(name)
        xs, as_ = self.x_strings.get(x, set()), self.a_strings.get(a, set())
        if not xs or not as_:
            return None
        return 'supported' if xs & as_ else 'contradicted'

    # ---- propagation to direct callees ----
    def propagate(self, named, rounds=8, max_gap=8):
        """Pair callees that sit in equal-length gaps between already-named callees."""
        known = {x: r['name'] for x, r in named.items()
                 if CONF_RANK[r['confidence']] >= 1 and 'aliases' not in r}
        used = set(known.values())
        added = {}
        for _ in range(rounds):
            votes = defaultdict(list)
            for x, name in list(known.items()):
                a = self.a.by_name.get(name)
                if a is None:
                    continue
                xc = collapse(self.x.calls(x))
                ac = collapse([t for t in self.a.calls(a) if not (self.a.name(t) or '').startswith(RUNTIME)])
                an = [self.a.name(t) for t in ac]
                if not xc or not ac:
                    continue
                anchors = self.lcs(xc, an, known)
                if not anchors and len(xc) != len(an):
                    continue
                bounds = [(-1, -1)] + anchors + [(len(xc), len(an))]
                for (i0, j0), (i1, j1) in zip(bounds, bounds[1:]):
                    gap = list(zip(range(i0 + 1, i1), range(j0 + 1, j1)))
                    if not gap or i1 - i0 != j1 - j0 or len(gap) > max_gap:
                        continue
                    if any(an[j] is None or (xc[i] in known and known[xc[i]] != an[j]) for i, j in gap):
                        continue
                    # the gap must look like the same functions in the same order
                    sims = [self.sim(xc[i], (ac[j],)) for i, j in gap]
                    if min(sims) < -1.5 or (len(gap) > 1 and sum(sims) < 0):
                        continue
                    inner = i0 >= 0 and i1 < len(xc)
                    strong = inner and (len(gap) == 1 or (len(gap) <= 3 and min(sims) >= -0.5))
                    for i, j in gap:
                        if xc[i] not in known:
                            votes[xc[i]].append((an[j], name, strong))
            claims = defaultdict(set)
            for target, vs in votes.items():
                cands = {v[0] for v in vs}
                if len(cands) == 1:
                    claims[next(iter(cands))].add(target)
            new = 0
            for target, vs in votes.items():
                cand = vs[0][0]
                if {v[0] for v in vs} != {cand} or cand in used or len(claims[cand]) != 1:
                    continue
                callers = sorted({v[1] for v in vs})
                if len(callers) < 2 and not any(v[2] for v in vs):
                    continue
                known[target] = cand
                used.add(cand)
                added[target] = {'name': cand, 'method': 'propagated', 'confidence': 'high',
                                 'android': hex(self.a.by_name[cand]) if cand in self.a.by_name else None,
                                 'contexts': callers[:8]}
                new += 1
            if not new:
                break
        return added

    @staticmethod
    def lcs(xc, an, known):
        n, m = len(xc), len(an)
        dp = [[0] * (m + 1) for _ in range(n + 1)]
        for i in range(n - 1, -1, -1):
            for j in range(m - 1, -1, -1):
                if an[j] and known.get(xc[i]) == an[j]:
                    dp[i][j] = dp[i + 1][j + 1] + 1
                else:
                    dp[i][j] = max(dp[i + 1][j], dp[i][j + 1])
        out, i, j = [], 0, 0
        while i < n and j < m:
            if an[j] and known.get(xc[i]) == an[j]:
                out.append((i, j))
                i, j = i + 1, j + 1
            elif dp[i + 1][j] >= dp[i][j + 1]:
                i += 1
            else:
                j += 1
        return out


def source_anchors(source_root=None):
    """`sub_XXXXXXXX ... // Class::Method` comments written by hand in src/."""
    pattern = re.compile(r'sub_([0-9A-Fa-f]{8})\)?;?\s*//\s*([A-Za-z_][A-Za-z_0-9]*)::(~?[A-Za-z_][A-Za-z_0-9]*)')
    found = {}
    for path in sorted((source_root or ROOT / 'src').glob('*')):
        if path.suffix not in ('.cpp', '.h'):
            continue
        for m in pattern.finditer(path.read_text(encoding='utf-8-sig')):
            found.setdefault(int(m.group(1), 16), (m.group(2), m.group(3), path.name))
    return found


def validate(named, namer, source_root=None):
    checks = []
    for addr, (cls, method, src) in sorted(source_anchors(source_root).items()):
        row = named.get(addr)
        if not row:
            checks.append({'xbox': hex(addr), 'source': '%s::%s (%s)' % (cls, method, src),
                           'derived': None, 'method': None, 'confidence': None,
                           'agrees': False, 'method_agrees': False})
            continue
        acls, amethod = split_name(row['name'])
        ok = method_key(amethod) == method and (acls.rsplit('::', 1)[-1].startswith(cls) or cls.startswith(acls.rsplit('::', 1)[-1]))
        checks.append({'xbox': hex(addr), 'source': '%s::%s (%s)' % (cls, method, src), 'derived': row['name'],
                       'method': row['method'], 'confidence': row['confidence'],
                       'agrees': ok, 'method_agrees': method_key(amethod) == method})
    # Widget input slots documented in scripts/audit_input_handlers.py
    widget = namer.x.vtables.get(('Sexy::Widget', 0), (0, []))[1]
    expect = {53: 'MouseMove(int, int)', 54: 'MouseDown(int, int, int, int)', 55: 'MouseDown(int, int, int)',
              56: 'MouseUp(int, int, int, int)', 57: 'MouseUp(int, int, int)', 58: 'MouseUp(int, int)',
              59: 'MouseDrag(int, int)', 60: 'MouseWheel(int)'}
    slots = []
    for i, method in expect.items():
        row = named.get(widget[i]) if i < len(widget) else None
        got = row['name'] if row else None
        slots.append({'slot': i, 'expected': 'Sexy::Widget::' + method, 'derived': got,
                      'aliases': row.get('aliases') if row else None,
                      'agrees': bool(row) and ('Sexy::Widget::' + method == got or
                                               'Sexy::Widget::' + method in row.get('aliases', []))})
    return checks, slots


def challenge_anchor(namer):
    path = OUT / 'challenge-tables.json'
    if not path.exists():
        return None
    import json
    t = json.loads(path.read_text(encoding='utf-8'))
    same = sum(1 for a, b in zip(t.get('android', []), t.get('xbox', []))
               if {k: v for k, v in a.items() if k != 'index'} == {k: v for k, v in b.items() if k != 'index'})
    return {'xbox_address': t.get('xbox_address'), 'android_address': t.get('android_address'),
            'entries': len(t.get('android', [])), 'identical': same}


def evaluate_golden(named, path, min_entries=40):
    """Evaluation only; called after all predictions are frozen."""
    if not path.exists():
        return {'available': False, 'path': path.as_posix()}
    golden = json.loads(path.read_text(encoding='utf-8'))
    checks, counts = [], defaultdict(Counter)
    addresses = set()
    for entry in golden['functions']:
        x = int(entry['xbox'], 16)
        if x in addresses or not entry.get('evidence'):
            raise ValueError('Duplicate or undocumented golden entry: ' + hex(x))
        addresses.add(x)
        row = named.get(x)
        level = row['confidence'] if row else 'missing'
        agrees = bool(row and row['name'] in [entry['name']] + entry.get('equivalent_names', []))
        counts[level]['evaluated'] += 1
        counts[level]['correct'] += agrees
        checks.append({'xbox': hex(x), 'expected': entry['name'], 'derived': row['name'] if row else None,
                       'confidence': level, 'agrees': agrees})
    metrics = {level: {**count, 'accuracy': count['correct'] / count['evaluated']}
               for level, count in sorted(counts.items())}
    negatives = []
    for entry in golden.get('negative_cases', []):
        row = named.get(int(entry['xbox'], 16))
        negatives.append({'xbox': entry['xbox'], 'rejected_name': entry['rejected_name'],
                          'passes': not row or row['confidence'] == 'low' or row['name'] != entry['rejected_name']})
    accepted = len(checks) >= min_entries and all(n['passes'] for n in negatives) and all(
        metrics.get(level, {}).get('evaluated', 0) > 0 and
        metrics[level]['accuracy'] >= threshold for level, threshold in [('high', 1.0), ('medium', 0.9)])
    return {'available': True, 'total': len(checks), 'by_confidence': metrics,
            'thresholds_met': accepted, 'checks': checks, 'negative_cases': negatives}


def input_hashes(golden_path, golden2_path=None, source_root=None):
    paths = [ROOT / '.cache/pvz-image.bin', ROOT / 'generated/default/pvz_funcs.h',
             OUT / 'android-symbols.json', ROOT / 'Chinese Android TV Image/lib/armeabi/libGameMain.so',
             Path(__file__).resolve(), ROOT / 'scripts/analyze_cross_platform.py',
             ROOT / 'scripts/confirm_xbox_vtables.py',
             ROOT / 'research/pvz-pc/SexyAppFramework/Widget.cpp',
             ROOT / 'research/pvz-pc/Lawn/GridItem.cpp']
    paths.extend(p for p in sorted((source_root or ROOT / 'src').glob('*')) if p.suffix in ('.cpp', '.h'))
    paths.extend(p for p in (OUT / 'challenge-tables.json', golden_path, golden2_path) if p and p.exists())
    return {p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(): sha(p.read_bytes())
            for p in sorted(set(paths))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=OUT / 'xbox-android-names.json')
    parser.add_argument('--csv', type=Path, default=ROOT / 'docs/native-port/data/xbox-names.csv')
    parser.add_argument('--golden', type=Path, default=ROOT / 'docs/native-port/data/name-map-golden.json')
    parser.add_argument('--golden2', type=Path, default=ROOT / 'docs/native-port/data/name-map-golden-2.json')
    parser.add_argument('--source-snapshot', type=Path, help='Read-only src snapshot for concurrent analyses; validation only')
    parser.add_argument('--without-t01b', action='store_true', help='Reproduce T01 baseline with unchanged predictors')
    parser.add_argument('--self-test', action='store_true', help='Check reachable-code exclusion of inline data')
    parser.add_argument('--inspect', type=lambda s: int(s, 16), help='Disassemble one Xbox function; writes no files')
    parser.add_argument('--android', type=lambda s: int(s, 16), help='Optional ARM counterpart for --inspect')
    parser.add_argument('--quiet', action='store_true', help='Print summary and golden metrics only')
    parser.add_argument('--check', action='store_true', help='Fail if golden thresholds, string checks or Widget dispatch checks fail')
    parser.add_argument('--check-t01b', action='store_true', help='Also require measurable high/medium precision on frozen golden-2')
    args = parser.parse_args()
    from confirm_xbox_vtables import confirm, self_test
    if args.self_test:
        self_test()
        return
    namer = Namer()
    if args.inspect is not None:
        x = args.inspect
        if x not in namer.x.fset:
            parser.error('--inspect must be a generated Xbox function start')
        print('xbox_sha256:', sha(namer.x.data))
        print('android_sha256:', sha(namer.a.data))
        cs = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
        code = namer.x.data[x - BASE:x - BASE + namer.x.size(x)]
        print('Xbox strings:', repr(namer.x.strings(x)))
        for ins in cs.disasm(code, x):
            print('%08x: %-9s %s' % (ins.address, ins.mnemonic, ins.op_str))
        if args.android is not None:
            a = args.android & ~1
            print('Android:', namer.a.name(a), 'strings:', repr(namer.a.strings(a)))
            code = namer.a.data[namer.a.off(a):namer.a.off(a) + namer.a.size[a]]
            for ins in namer.a.cs.disasm(code, a):
                print('%08x: %-9s %s' % (ins.address, ins.mnemonic, ins.op_str))
        return
    namer.index_strings()
    namer.string_pairs = {}
    namer.name_vtables()
    namer.independent_vtables = {x: namer.resolve(ev) for x, ev in namer.evidence.items()}
    namer.columns, namer.colscore, namer.class_rows = {}, {}, []
    namer.evidence.clear()
    namer.string_pairs = namer.name_by_strings()
    namer.name_vtables()
    named = {x: namer.resolve(ev) for x, ev in namer.evidence.items()}
    for x, row in namer.string_pairs.items():
        old = named.get(x)
        if old is None:
            named[x] = row
        elif old['name'] == row['name']:
            row['vtable_agrees'] = old['method'] == 'vtable' and not old.get('aliases')
            row['evidence'].append({'kind': 'vtable-candidate', 'contexts': old['contexts']})
            named[x] = row
        elif old['name'] != row['name']:
            row['evidence'].append({'kind': 'vtable-disagrees', 'name': old['name'], 'contexts': old['contexts']})
            named[x] = row
    # Gap propagation remains disabled: similar call counts are not identity.
    namer.corroborate(named)
    confirmation = confirm(namer, named) if not args.without_t01b else {'disabled': True}
    checks, widget_slots = validate(named, namer, args.source_snapshot)

    rows, string_checks = [], defaultdict(Counter)
    for x in sorted(named):
        r = named[x]
        if 'aliases' not in r:
            check = namer.string_check(x, r['name'])
            if check:
                r['strings'] = check
                string_checks['%s/%s' % (r['method'], r['confidence'])][check] += 1
        rows.append({'xbox': hex(x), 'size': namer.x.size(x), **r})
    per_class = []
    for cls in namer.classes:
        if (cls, 0) not in namer.x.vtables:
            continue
        xslots = namer.x.vtables[(cls, 0)][1]
        distinct = set(xslots)
        named_here = {x for x in distinct if x in named}
        high = {x for x in named_here if named[x]['confidence'] == 'high'}
        per_class.append({'class': cls, 'xbox_vtable': hex(namer.x.vtables[(cls, 0)][0]),
                          'xbox_slots': len(xslots), 'android_slots': len(namer.a_slots(cls)),
                          'aligned_slots': len(namer.columns[cls]), 'distinct_functions': len(distinct),
                          'named': len(named_here), 'named_high': len(high)})
    by_method = Counter(r['method'] for r in named.values())
    by_conf = Counter(r['confidence'] for r in named.values())
    agree = sum(c['agrees'] for c in checks)
    xbox_classes = {c for c, _ in namer.x.vtables}
    summary = {
        'xbox_rtti_classes': len(set(namer.x.types.values())),
        'xbox_classes_with_vtable': len(xbox_classes),
        'android_classes_with_vtable': len(namer.a.vtables),
        'matched_classes': len(per_class),
        'xbox_vtable_slots_matched_classes': sum(r['xbox_slots'] for r in per_class),
        'xbox_vtable_slots_aligned': sum(r['aligned_slots'] for r in per_class),
        'named_functions': len(named), 'by_method': dict(by_method), 'by_confidence': dict(by_conf),
        'xbox_functions_total': len(namer.x.funcs),
        'source_anchor_checks': len(checks), 'source_anchor_agree': agree,
        'source_anchor_method_agree': sum(c['method_agrees'] for c in checks),
        'widget_input_slots_agree': sum(s['agrees'] for s in widget_slots),
        'string_overlap_checks': {k: dict(v) for k, v in sorted(string_checks.items())},
    }
    hashes = input_hashes(args.golden, args.golden2, args.source_snapshot)
    manifest_hash = sha(json.dumps(hashes, sort_keys=True).encode('utf-8'))
    golden = evaluate_golden(named, args.golden)
    golden2 = evaluate_golden(named, args.golden2, min_entries=30)
    if golden.get('available') and golden2.get('available'):
        overlap = sorted({r['xbox'] for r in golden['checks']} & {r['xbox'] for r in golden2['checks']})
        from confirm_xbox_vtables import CONSOLE_CLASSES
        console_count = sum(c['expected'].split('(')[0].rsplit('::', 1)[0] in CONSOLE_CLASSES
                            for c in golden2['checks'])
        golden2.update({'overlap_with_T01': overlap, 'requested_class_functions': console_count})
        golden2['thresholds_met'] &= not overlap and console_count >= 15
    save_json(args.output, {
        'inputs': hashes,
        'input_manifest_sha256': manifest_hash,
        'method': __doc__.strip().splitlines()[0],
        'summary': summary,
        'validation': {'challenge_table': challenge_anchor(namer), 'widget_input_slots': widget_slots,
                       'source_comments': checks, 'golden': golden, 'golden2': golden2},
        'T01b_confirmation': confirmation,
        'excluded_xbox_classes': sorted(c for c in xbox_classes if c.startswith(('std::', 'excluded-msvc-template:'))),
        'unmatched_xbox_classes': sorted(c for c in xbox_classes - set(namer.a.vtables)
                                         if not c.startswith(('std::', 'excluded-msvc-template:'))),
        'alignment': namer.class_rows,
        'classes': per_class,
        'rejected_string_pairs': namer.rejected_string_pairs,
        'functions': rows})

    args.csv.parent.mkdir(parents=True, exist_ok=True)
    xbox_hash = hashes['.cache/pvz-image.bin']
    android_hash = hashes['Chinese Android TV Image/lib/armeabi/libGameMain.so']
    with args.csv.open('w', encoding='utf-8', newline='') as stream:
        fields = ['xbox_address', 'size', 'name', 'confidence', 'method', 'android_address', 'evidence']
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator='\n')
        writer.writeheader()
        for row in rows:
            if row['confidence'] not in ('high', 'medium'):
                continue
            writer.writerow(dict(zip(fields, [row['xbox'], row['size'], row['name'], row['confidence'],
                                              row['method'], row['android'],
                                              json.dumps({'input_manifest_sha256': manifest_hash,
                                                  'xbox_image_sha256': xbox_hash,
                                                  'android_sha256': android_hash,
                                                  'signals': row['evidence']}, ensure_ascii=False, sort_keys=True)])))

    if not args.quiet:
        print('%-34s %5s %5s %5s %5s %5s' % ('class', 'xbox', 'andr', 'align', 'named', 'high'))
        for r in per_class:
            print('%-34s %5d %5d %5d %5s %5d' % (r['class'][:34], r['xbox_slots'], r['android_slots'],
                                                 r['aligned_slots'], '%d/%d' % (r['named'], r['distinct_functions']),
                                                 r['named_high']))
    print()
    for k, v in summary.items():
        print('%s: %s' % (k, v))
    print('golden: %s' % json.dumps({k: v for k, v in golden.items() if k != 'checks'}, ensure_ascii=True))
    print('golden2: %s' % json.dumps({k: v for k, v in golden2.items() if k != 'checks'}, ensure_ascii=True))
    print('T01b promotions: %s' % json.dumps({k: v for k, v in confirmation.items() if k != 'tested'}, ensure_ascii=True))
    if (args.check or args.check_t01b) and (not golden.get('thresholds_met')
                       or (args.check_t01b and not golden2.get('thresholds_met')) or not all(s['agrees'] for s in widget_slots)
                       or any(r['confidence'] == 'high' and r.get('strings') == 'contradicted' for r in rows)):
        raise SystemExit('T01 validation failed (see JSON validation/golden and string checks)')
    if args.quiet:
        return
    bad = [c for c in checks if not c['agrees']]
    for c in bad:
        print('MISMATCH %s src=%s derived=%s (%s, %s)' % (c['xbox'], c['source'], c['derived'], c['method'], c['confidence']))
    for s in widget_slots:
        if not s['agrees']:
            print('WIDGET SLOT %d expected %s got %s' % (s['slot'], s['expected'], s['derived']))


if __name__ == '__main__':
    main()
