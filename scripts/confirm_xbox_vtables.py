"""T01b: corroborate frozen vtable candidates without consulting either golden.

Inputs: Namer's read-only PPC/Thumb images and a frozen copy of T01 predictions.
Output: evidence/promotions only, applied once by name_xbox_from_android.py.
Conservative reachable-code traversal stops at unresolved indirect dispatch/table
jumps. It never decodes inline jump tables or literal pools as instructions.
One globally unique nontrivial integer or ordered pair of integers, exact calls to
two frozen seeds, or a call at an anchored ordinal in a frozen named caller is a
second signal. Two different signal families permit high; otherwise medium.
Stack sizes, memory offsets, addresses, sizes and empty bodies are not evidence.
"""
from collections import defaultdict
from capstone import Cs, CS_ARCH_ARM, CS_ARCH_PPC, CS_MODE_ARM, CS_MODE_THUMB, CS_MODE_32, CS_MODE_BIG_ENDIAN

TRIVIAL = {0, 1, -1, 0xffffffff} | {1 << n for n in range(9)}
CONSOLE_CLASSES = {'GamepadControls', 'BaseGamepadControls', 'ZenGardenControls',
                   'TreeOfWisdomControls', 'MainMenu', 'VSSetupMenu',
                   'LawnMenuWidget', 'Sexy::Widget'}


def reachable_ppc(data, address):
    """No indirect target speculation; return deterministic address-ordered code."""
    cs = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
    pending, seen = [address], {}
    end = address + len(data)
    while pending:
        p = pending.pop()
        if p in seen or not address <= p <= end - 4 or (p - address) % 4:
            continue
        ins = next(cs.disasm(data[p-address:p-address+4], p, count=1), None)
        if ins is None:
            continue
        seen[p] = ins
        w = int.from_bytes(ins.bytes, 'big')
        op = w >> 26
        if op == 18:  # b/bl; LK distinguishes call from intra-function jump.
            d = w & 0x3fffffc
            d = d - 0x4000000 if d & 0x2000000 else d
            target = d if w & 2 else p + d
            pending.append(p + 4 if w & 1 else target)
        elif op == 16:
            d = w & 0xfffc
            d = d - 0x10000 if d & 0x8000 else d
            pending.extend([p + 4, d if w & 2 else p + d])
        elif op == 19 and ((w >> 1) & 1023) in (16, 528):
            # bclr/bcctr: conditional returns can fall through, bctrl is a call.
            if w & 1 or ((w >> 21) & 31) != 20:
                pending.append(p + 4)
        else:
            pending.append(p + 4)
    return [seen[p] for p in sorted(seen)]


def reachable_arm(data, address, thumb=True):
    cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB if thumb else CS_MODE_ARM)
    pending, seen = [address], {}
    end = address + len(data)
    while pending:
        p = pending.pop()
        if p in seen or not address <= p < end or (p - address) % 2:
            continue
        ins = next(cs.disasm(data[p-address:p-address+4], p, count=1), None)
        if ins is None or p + ins.size > end:
            continue
        seen[p] = ins
        mn, operand = ins.mnemonic.removesuffix('.w'), ins.op_str
        nxt = p + ins.size
        if mn in ('tbb', 'tbh') or (mn in ('pop', 'ldm', 'ldmia') and 'pc' in operand):
            continue
        if mn == 'bx':
            continue
        if mn in ('bl', 'blx'):
            pending.append(nxt)
        elif mn == 'b':
            if operand.startswith('#'):
                pending.append(int(operand[1:], 0))
        elif mn in ('cbz', 'cbnz') or (mn.startswith('b') and operand.startswith('#')):
            pending.extend([nxt, int(operand.rsplit('#', 1)[-1], 0)])
        elif ('pc' in operand.split(',')[0] and mn.startswith(('mov', 'ldr', 'add'))):
            continue
        else:
            pending.append(nxt)
    return [seen[p] for p in sorted(seen)]


def features(namer, platform, address):
    """Integers only in literal value/compare roles, never displacement/stack roles."""
    if platform == 'xbox':
        code = namer.x.data[address-0x82000000:address-0x82000000+namer.x.size(address)]
        instructions = reachable_ppc(code, address)
    else:
        off = namer.a.off(address)
        code = namer.a.data[off:off+namer.a.size[address]]
        thumb = address in namer.a.thumb_addresses
        instructions = reachable_arm(code, address, thumb)
    constants, roles, calls = defaultdict(list), defaultdict(lambda: defaultdict(list)), []
    for ins in instructions:
        mnemonic, operands = ins.mnemonic.removesuffix('.w'), ins.op_str
        value, role = None, None
        if platform == 'xbox':
            w = int.from_bytes(ins.bytes, 'big')
            op, ra = w >> 26, (w >> 16) & 31
            # li r1 is an unusual stack assignment: exclude stack registers.
            if (op == 14 and ra == 0 and ((w >> 21) & 31) != 1) or op in (10, 11):
                value = w & 0xffff
                if op != 10 and value & 0x8000:
                    value -= 0x10000
                role = 'literal' if op == 14 else 'compare'
            if op == 18 and w & 3 == 1:
                d = w & 0x3fffffc
                d = d - 0x4000000 if d & 0x2000000 else d
                target = ins.address + d
                # Generated ABI register spill/restore helpers, not game calls.
                if not 0x825fc800 <= target < 0x825fd000:
                    calls.append((ins.address, target))
        else:
            # MOVW alone can be the low half of a relocated address: exclude it
            # instead of pretending that an unreconstructed halfword is an ID.
            if mnemonic in ('mov', 'movs', 'cmp', 'cmn') and '#' in operands:
                destination = operands.split(',')[0]
                if destination not in ('sp', 'pc'):
                    value = int(operands.rsplit('#', 1)[1], 0)
                    role = 'compare' if mnemonic in ('cmp', 'cmn') else 'literal'
            if mnemonic in ('bl', 'blx') and operands.startswith('#'):
                target = int(operands[1:], 0) & ~1
                name = namer.a.name(target) or ''
                if not name.startswith(('__aeabi', '__gnu', '__cxa', '_Unwind')):
                    calls.append((ins.address, target))
        # Addresses and huge masks are not resource/button/mode integer IDs.
        if value is not None and value not in TRIVIAL and -0x8000 <= value <= 0xffff:
            constants[value].append(hex(ins.address))
            roles[value][role].append(ins.address)
    return {'constants': dict(constants), 'roles': {c: dict(r) for c, r in roles.items()},
            'calls': calls, 'reachable_instructions': len(instructions)}


def confirm(namer, named):
    namer.a.thumb_addresses = {s['value'] & ~1 for s in namer.a.syms
                               if s['type'] == 2 and s['value'] & 1}
    # This snapshot predates ALL T01b promotions, including confidence changes.
    seeds = {x: int(r['android'], 16) for x, r in named.items()
             if r['confidence'] in ('high', 'medium') and not r.get('aliases')}
    candidates = {x: r for x, r in named.items()
                  if r['method'] == 'vtable' and not r.get('aliases')}
    # Uniqueness universe: all generated Xbox functions / all named ARM functions.
    # This is deliberately stricter than uniqueness among an aligned class only.
    xf = {x: features(namer, 'xbox', x) for x in namer.x.funcs}
    af = {a: features(namer, 'android', a) for a in sorted(namer.a.size)}
    xu, au = defaultdict(set), defaultdict(set)
    for x, f in xf.items():
        for c in f['constants']:
            xu[c].add(x)
    for a, f in af.items():
        for c in f['constants']:
            au[c].add(a)
    incoming = defaultdict(list)
    seed_targets = set(seeds.values())
    for caller, counterpart in sorted(seeds.items()):
        xc, ac = xf[caller]['calls'], af[counterpart]['calls']
        if len(xc) != len(ac):
            continue
        anchors = [i for i, ((_, xt), (_, at)) in enumerate(zip(xc, ac))
                   if xt in seeds and seeds[xt] == at]
        if not anchors or any(xt in seeds and seeds[xt] != at
                              for (_, xt), (_, at) in zip(xc, ac)):
            continue
        for i, ((xp, xt), (ap, at)) in enumerate(zip(xc, ac)):
            if xt in candidates and int(candidates[xt]['android'], 16) == at and xt != caller:
                incoming[xt].append({'caller_xbox': hex(caller), 'caller_android': hex(counterpart),
                    'ordinal': i, 'direct_call_count': len(xc), 'anchor_ordinals': anchors,
                    'call_xbox': hex(xp), 'call_android': hex(ap)})
    promoted, tested = [], []
    for x, row in sorted(candidates.items()):
        if row.get('strings') == 'contradicted' or namer.string_check(x, row['name']) == 'contradicted':
            continue
        a = int(row['android'], 16)
        signals = []
        unique = sorted(c for c in xf[x]['constants'].keys() & af[a]['constants'].keys()
                        if xu[c] == {x} and au[c] == {a})
        if unique:
            signals.append({'kind': 'T01b-unique-semantic-integers',
                'universe': 'all generated PPC / all defined named ARM functions',
                'constants': [{'value': c, 'xbox_instructions': xf[x]['constants'][c],
                               'android_instructions': af[a]['constants'][c]} for c in unique]})
        else:
            # A combination can be unique even when its individual resource/mode
            # IDs are used elsewhere. Require both IDs in the same roles and
            # order, and exactly one containing function in EACH full binary.
            # This is one constant-family signal, never two high signals.
            common = sorted(xf[x]['constants'].keys() & af[a]['constants'].keys())
            pair = None
            for i, c in enumerate(common):
                for d in common[i+1:]:
                    if xu[c] & xu[d] != {x} or au[c] & au[d] != {a}:
                        continue
                    for cr in sorted(xf[x]['roles'][c].keys() & af[a]['roles'][c].keys()):
                        for dr in sorted(xf[x]['roles'][d].keys() & af[a]['roles'][d].keys()):
                            xp = [min(xf[x]['roles'][c][cr]), min(xf[x]['roles'][d][dr])]
                            ap = [min(af[a]['roles'][c][cr]), min(af[a]['roles'][d][dr])]
                            if (xp[0] < xp[1]) == (ap[0] < ap[1]):
                                pair = {'values': [c, d], 'roles': [cr, dr],
                                        'xbox_instructions': list(map(hex, xp)),
                                        'android_instructions': list(map(hex, ap))}
                                break
                        if pair:
                            break
                    if pair:
                        break
                if pair:
                    break
            if pair:
                signals.append({'kind': 'T01b-unique-semantic-integer-pair',
                    'universe': 'all generated PPC / all defined named ARM functions', **pair})
        xs = list(dict.fromkeys(seeds[t] for _, t in xf[x]['calls'] if t in seeds and t != x))
        ars = list(dict.fromkeys(t for _, t in af[a]['calls'] if t in seed_targets and t != a))
        if len(xs) >= 2 and xs == ars:
            signals.append({'kind': 'T01b-frozen-ordered-callees', 'android': list(map(hex, xs)),
                            'xbox': [hex(next(k for k, v in seeds.items() if v == t)) for t in xs]})
        if incoming[x]:
            signals.append({'kind': 'T01b-frozen-caller-ordinal', 'callers': incoming[x]})
        # A pre-existing T01 ordered-callee proof remains a separate family.
        # Count it only if this pass adds constants/caller evidence; never count
        # the same callee-order proof twice, and never use a promoted row here.
        previous_calls = next((e for e in row['evidence']
                               if e['kind'] == 'ordered-string-seed-callees'), None)
        if signals and previous_calls and not any(e['kind'] == 'T01b-frozen-ordered-callees' for e in signals):
            signals.append({'kind': 'T01b-frozen-T01-ordered-callees',
                            'android': previous_calls['android']})
        tested.append({'xbox': hex(x), 'name': row['name'], 'signals': len(signals),
                       'reachable_xbox': xf[x]['reachable_instructions'],
                       'reachable_android': af[a]['reachable_instructions']})
        if not signals:
            continue
        old = row['confidence']
        # Never downgrade a pre-existing T01 export in this independent pass.
        new = 'high' if len(signals) >= 2 else 'medium'
        if old == 'high':
            new = old
        row['evidence'].extend(signals)
        row['confidence'] = new
        if old != new:
            promoted.append({'xbox': hex(x), 'name': row['name'], 'from': old, 'to': new,
                             'signal_kinds': [s['kind'] for s in signals]})
    return {'frozen_seeds': len(seeds), 'seed_policy': 'T01 high/medium before any T01b promotion',
            'recursive': False, 'tested': tested, 'promoted': promoted,
            'console_new_names': sum(r['from'] == 'low' and
                r['name'].split('(')[0].rsplit('::', 1)[0] in CONSOLE_CLASSES for r in promoted)}


def self_test():
    # PPC branch skips an apparent li42 in data; real li43 remains reachable.
    p = reachable_ppc(bytes.fromhex('480000083860002a3860002b4e800020'), 0x1000)
    assert [i.address for i in p] == [0x1000, 0x1008, 0x100c]
    # Thumb b skips apparent movs42; movs43/bx lr retained, trailing pool excluded.
    a = reachable_arm(bytes.fromhex('00e02a202b2070472c20'), 0x2000)
    assert [i.address for i in a] == [0x2000, 0x2004, 0x2006]
    a32 = reachable_arm(bytes.fromhex('2a00a0e31eff2fe12b00a0e3'), 0x3000, thumb=False)
    assert [i.address for i in a32] == [0x3000, 0x3004]
    print('T01b reachable-code self-test: 3/3 passed')
