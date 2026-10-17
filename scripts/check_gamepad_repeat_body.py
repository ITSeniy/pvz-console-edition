"""Guard for the hand-specialized copy of 82580D88 in src/gamepad_repeat.cpp.

The T07b fix copies the generated body of Sexy::Gamepad::Update (82580D88)
and wraps only its repeat loop. If codegen changes that function, the copy
silently goes stale. This check takes the copy, removes the declared
insertions (each at its declared place, in order) and requires the rest to be
line-for-line identical to the current generated body.

Exit codes: 0 match, 1 mismatch, 77 generated/ is absent (CTest skip).
"""
import argparse
import sys
from pathlib import Path

ADDRESS = '82580D88'
NATIVE = Path('src/gamepad_repeat.cpp')
GENERATED_GLOB = 'generated/default/pvz_recomp.*.cpp'
SKIP = 77

ANCHOR_CALL = '\tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);'
# (inserted lines, generated line before them, generated line after them)
INSERTIONS = [
    (('// Only the counter path is clocked by completed root logic updates.',
      '// Keep the scan/bookkeeping on zero-tick polls. Input polling above and',
      '// ALL edge/axis/disconnection code below remain unchanged.',
      'for (uint32_t tick = 0; tick < (repeat_steps ? repeat_steps : 1u); ++tick) {',
      '    ctx.r30.u64 = 0;'),
     '\tctx.r27.s64 = ctx.r31.s64 + 40;', 'loc_82580DDC:'),
    (('    if (repeat_steps == 0) goto loc_82580EE4;',),
     '\tif (ctx.cr0.eq) goto loc_82580EE4;', '\t// addi r11,r30,15'),
    (('\tTraceRepeat(ctx, base);',), '\tctx.lr = 0x82580E60;', ANCHOR_CALL),
    (('    TraceResult(ctx, base);',), ANCHOR_CALL, 'loc_82580E60:'),
    (('\tTraceRepeat(ctx, base);',), '\tctx.lr = 0x82580EE4;', ANCHOR_CALL),
    (('    TraceResult(ctx, base);',), ANCHOR_CALL, 'loc_82580EE4:'),
    (('}', ''), '\tif (ctx.cr6.lt) goto loc_82580DDC;', '\t// li r30,4'),
    (('    After(poll_before, base);',), '\t__restgprlr_26(ctx, base);', '\treturn;'),
]


class Mismatch(Exception):
    pass


def generated_body(root):
    start = f'DEFINE_REX_FUNC(sub_{ADDRESS}) {{'
    for path in sorted(root.glob(GENERATED_GLOB)):
        lines = path.read_text(encoding='utf-8').splitlines()
        if start in lines:
            i = lines.index(start) + 1
            end = lines.index('}', i)
            return path, lines[i:end]
    return None, None


def native_body(text):
    """Hook lines from REX_FUNC_PROLOGUE to the hook's closing brace."""
    lines = text.splitlines()
    hook = lines.index(f'REX_HOOK_RAW(sub_{ADDRESS}) {{')
    depth = 0
    for end in range(hook, len(lines)):
        depth += lines[end].count('{') - lines[end].count('}')
        if depth == 0 and end > hook:
            break
    else:
        raise Mismatch('hook has no closing brace')
    body = lines[hook + 1:end]
    first = body.index('\tREX_FUNC_PROLOGUE();')
    return body[first:]


def compare(native, generated):
    declared = {line for block, _, _ in INSERTIONS for line in block}
    clash = declared & set(generated)
    if clash:
        raise Mismatch(f'declared insertion also occurs in generated body: {sorted(clash)}')
    i = j = k = 0
    while i < len(native):
        if k < len(INSERTIONS):
            block, before, after = INSERTIONS[k]
            if tuple(native[i:i + len(block)]) == block:
                prev = generated[j - 1] if j else None
                nxt = generated[j] if j < len(generated) else None
                if (prev, nxt) != (before, after):
                    raise Mismatch(f'insertion {k + 1} ({block[0].strip()!r}) at native line +{i} '
                                   f'sits between {prev!r} and {nxt!r}, declared {before!r} / {after!r}')
                i += len(block)
                k += 1
                continue
        if j >= len(generated):
            raise Mismatch(f'extra native line +{i}: {native[i]!r}')
        if native[i] != generated[j]:
            raise Mismatch(f'native line +{i} {native[i]!r} != generated line +{j} {generated[j]!r}')
        i += 1
        j += 1
    if k != len(INSERTIONS):
        raise Mismatch(f'declared insertion {k + 1} ({INSERTIONS[k][0][0].strip()!r}) not found')
    if j != len(generated):
        raise Mismatch(f'native copy ends early; generated line +{j} {generated[j]!r} missing')


def self_test(native, generated):
    compare(native, generated)
    loop_cmp = '\tctx.cr6.compare<int32_t>(ctx.r30.s32, 20, ctx.xer);'
    skip = '    if (repeat_steps == 0) goto loc_82580EE4;'
    at = native.index(skip)
    mutations = {
        'changed operand': [l.replace(', 20,', ', 21,') if l == loop_cmp else l for l in native],
        'dropped line': native[:40] + native[41:],
        'undeclared line': native[:40] + ['\tctx.r3.u64 = 0;'] + native[40:],
        'moved insertion': native[:at] + [native[at + 1], native[at]] + native[at + 2:],
        'missing insertion': [l for l in native if l != '    After(poll_before, base);'],
        'stale generated': generated[:-3] + ['\tctx.r4.s64 = 0;'] + generated[-3:],
    }
    for name, mutated in mutations.items():
        gen = mutated if name == 'stale generated' else generated
        nat = native if name == 'stale generated' else mutated
        try:
            compare(nat, gen)
        except Mismatch:
            continue
        raise Mismatch(f'self-test: {name} was not detected')
    print(f'self-test: {len(mutations)} mutations detected')


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    path, generated = generated_body(args.root)
    if generated is None:
        print(f'SKIP: sub_{ADDRESS} not found under {GENERATED_GLOB} (run codegen first)')
        return SKIP
    native = native_body((args.root / NATIVE).read_text(encoding='utf-8'))
    try:
        if args.self_test:
            self_test(native, generated)
        compare(native, generated)
    except Mismatch as e:
        print(f'FAIL: {NATIVE} copy of sub_{ADDRESS} differs from {path.relative_to(args.root)}: {e}')
        return 1
    print(f'OK: {NATIVE} matches {path.relative_to(args.root)} '
          f'({len(generated)} lines, {len(INSERTIONS)} declared insertions)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
