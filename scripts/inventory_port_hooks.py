"""Inventory native-port intervention sites without changing runtime code.

Inputs: src/*.cpp,h; config/analysis.toml; SDK hook.h; existing docs/*.md.
Outputs: docs/native-port/data/hook-inventory.csv and hook-inventory.md.
Lexical C++ scanning preserves source coordinates while masking comments/strings.
Original-call policy follows if/else/return paths, including function-pointer
forwarding to local helpers. Loops/switches are conservative (zero/one iteration).
This is source evidence, not proof of runtime reachability or replacement safety.
No T01 names are consumed until that task is independently accepted.
"""
from pathlib import Path
from collections import Counter
import argparse
import bisect
import csv
import hashlib
import json
import re
import tomllib

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'docs/native-port'
SDK_HOOK = ROOT / 'research/rexglue-source/include/rex/hook.h'
TOKEN = re.compile(r'R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})\([\s\S]*?\)(?P=delimiter)"|'
                   r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
SYMBOL = re.compile(r'(?<![\w])(?:__imp__)?sub_([0-9A-Fa-f]{8})(?!\w)')
ADDRESS = re.compile(r'\b0x82[0-9a-fA-F]{6}\b')
READS = re.compile(r'\b(?:Read|Write|Float|ReadDouble|REX_(?:LOAD|STORE)_\w+)\s*\(')
DEFAULTS = {
    'audio_fixes.cpp': 'audio', 'audio_diagnostics.cpp': 'audio',
    'camera_compat.cpp': 'platform', 'windows_identity.cpp': 'platform',
    'startup_video.cpp': 'platform', 'bonus_games.cpp': 'menus',
    'game_settings.cpp': 'menus', 'pc_menus.cpp': 'menus',
    'controller_assets.cpp': 'render', 'seed_image_fixes.cpp': 'render',
    'pc_game_fixes.cpp': 'gameplay', 'service_mouse.cpp': 'input',
    'mouse_bridge.cpp': 'input', 'challenge_mouse.cpp': 'input',
}
FIELDS = ['kind', 'file', 'line', 'end_line', 'symbol', 'guest_address', 'subsystem',
          'source_name', 'name_confidence', 'original_policy', 'original_position',
          'original_evidence', 'docs', 'expression', 'offsets', 'evidence', 'input_manifest_sha256']


def mask(text):
    return TOKEN.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m.group()), text)


def balanced(code, at, opening='(', closing=')'):
    if at >= len(code) or code[at] != opening:
        raise ValueError('Expected %s at %d' % (opening, at))
    depth = 0
    for i in range(at, len(code)):
        depth += code[i] == opening
        depth -= code[i] == closing
        if depth == 0:
            return i + 1
    raise ValueError('Unclosed %s at %d' % (opening, at))


def arguments(code, at):
    end = balanced(code, at)
    result, start, depth = [], at + 1, 0
    for i in range(at + 1, end - 1):
        depth += code[i] in '([{<'
        depth -= code[i] in ')]}>'
        if code[i] == ',' and depth == 0:
            result.append((start, i))
            start = i + 1
    result.append((start, end - 1))
    return result, end


class Flow:
    """Possible (called_original, returned) paths through a masked C++ body."""
    def __init__(self, code, original):
        self.code = code
        self.original = re.compile(r'\b' + re.escape(original) + r'\s*\(')
        self.approximated = set()

    def skip(self, at):
        while at < len(self.code) and self.code[at].isspace():
            at += 1
        return at

    def called(self, text, states):
        hit = bool(self.original.search(text))
        return {(old or hit, done) if not done else (old, done) for old, done in states}

    def statement(self, at, states):
        at = self.skip(at)
        if at >= len(self.code):
            return at, states
        if self.code[at] == '{':
            end = balanced(self.code, at, '{', '}')
            return end, self.sequence(at + 1, end - 1, states)
        word = re.match(r'[A-Za-z_]\w*', self.code[at:])
        key = word.group() if word else ''
        next_at = self.skip(at + len(key))
        if key in ('if', 'for', 'while', 'switch') and self.code[next_at:next_at+1] == '(':
            cond_end = balanced(self.code, next_at)
            incoming = self.called(self.code[next_at:cond_end], states)
            end, yes = self.statement(cond_end, incoming)
            rest = self.skip(end)
            if key == 'if':
                if re.match(r'else\b', self.code[rest:]):
                    end, no = self.statement(rest + 4, incoming)
                else:
                    no = incoming
                return end, yes | no
            self.approximated.add(key)
            return end, incoming | yes
        # Generic statement: balanced initializers/calls, ending at semicolon.
        end = at
        while end < len(self.code):
            ch = self.code[end]
            if ch in '([{':
                end = balanced(self.code, end, ch, {'(': ')', '[': ']', '{': '}'}[ch])
            elif ch == ';':
                end += 1
                break
            elif ch == '}':
                break
            else:
                end += 1
        if end <= at:
            return at + 1, states
        out = self.called(self.code[at:end], states)
        if key in ('return', 'throw'):
            out = {(called, True) for called, _ in out}
        return end, out

    def sequence(self, at, end, states):
        while self.skip(at) < end:
            returned = {s for s in states if s[1]}
            active = states - returned
            at, active = self.statement(at, active)
            states = returned | active
        return states

    def policy(self):
        states = self.sequence(0, len(self.code), {(False, False)})
        outcomes = {s[0] for s in states}
        return 'always' if outcomes == {True} else 'never' if outcomes == {False} else 'conditional'


def source_file(path):
    raw = path.read_text(encoding='utf-8-sig')
    code = mask(raw)
    newlines = [i for i, c in enumerate(raw) if c == '\n']
    return raw, code, lambda at: bisect.bisect_left(newlines, at) + 1


def helpers(code):
    found = {}
    pat = re.compile(r'\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\{')
    for m in pat.finditer(code):
        if m[1] in ('if', 'for', 'while', 'switch', 'catch', 'REX_HOOK_RAW'):
            continue
        start = m.end() - 1
        end = balanced(code, start, '{', '}')
        ranges, _ = arguments(code, code.index('(', m.start()))
        params = [re.findall(r'\b[A-Za-z_]\w*\b', code[a:b]) for a, b in ranges]
        found[m[1]] = ([p[-1] if p else '' for p in params], code[start+1:end-1], start)
    return found


def original_calls(symbol, body, local_helpers, absolute, line):
    original = '__imp__' + symbol if not symbol.startswith('__imp__') else None
    # SDK imports have no recompiled __imp__sub body. Record native forwarding
    # separately in evidence; do not invent an original guest-function call.
    if not original:
        return 'never', 'none', {'reason': 'SDK import replacement; inspect native delegation in body'}
    direct = list(re.finditer(r'\b' + re.escape(original) + r'\s*\(', body))
    if direct:
        flow = Flow(body, original)
        policy = flow.policy()
        prefix = body[:direct[0].start()].strip(' \r\n\t{}')
        suffix = body[balanced(body, body.index('(', direct[-1].start())):].strip(' \r\n\t{};')
        suffix = re.sub(r'\breturn\s*;?', '', suffix).strip()
        position = 'around' if prefix and suffix else 'after' if prefix else 'before' if suffix else 'only'
        return policy, position, {'original_symbol': original,
            'call_lines': [line(absolute + m.start()) for m in direct],
            'flow_overapproximation': sorted(flow.approximated)}
    if re.search(r'\b' + re.escape(original) + r'\b', body):
        for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(', body):
            ranges, _ = arguments(body, body.index('(', m.start()))
            for index, (a, b) in enumerate(ranges):
                if body[a:b].strip() != original or m[1] not in local_helpers:
                    continue
                params, helper_body, helper_start = local_helpers[m[1]]
                param = params[index]
                flow = Flow(helper_body, param)
                policy = flow.policy()
                return policy, 'helper', {'original_symbol': original, 'via': m[1], 'parameter': param,
                    'helper_line': line(helper_start), 'forward_line': line(absolute + a),
                    'flow_overapproximation': sorted(flow.approximated)}
        raise ValueError('Unresolved original function pointer forwarding: ' + symbol)
    return 'never', 'none', {'original_symbol': original, 'reason': 'no original reference in hook body'}


def subsystem(path, symbol, name):
    address = SYMBOL.fullmatch(symbol)
    n = int(address[1], 16) if address else 0
    if path.name == 'mouse_bridge.cpp' and symbol == 'sub_821ABDE8':
        return 'audio'
    if '::Draw' in name or symbol in ('sub_82196600',):
        return 'render'
    if path.name in ('mouse_bridge.cpp', 'challenge_mouse.cpp') and (
            0x82490000 <= n < 0x824A0000 or '::Draw' in name or
            symbol in ('sub_82146A60', 'sub_82188310', 'sub_82284D98')):
        return 'render'
    return DEFAULTS.get(path.name, 'support')


def self_test():
    fixtures = [
        ('original();', 'always'),
        ('if (mouse) return; original();', 'conditional'),
        ('if (mouse) { original(); return; } original();', 'always'),
        ('if (mouse) original(); else original();', 'always'),
        ('original(); if (mouse) return;', 'always'),
        ('if (mouse) { original(); return; } return;', 'conditional'),
        ('for (int i=0; i<n; ++i) original();', 'conditional'),
        ('if (original()) return;', 'always'),
        ('// original();\n const char* x="original();}"; return;', 'never'),
        ('auto x=R"tag( original(); { })tag"; original();', 'always'),
        ('if (a) { if(b) return; original(); } else original();', 'conditional'),
        ('if (a) { draw(ctx,b); return; } draw(ctx,b);', 'always'),
    ]
    for index, (source, expected) in enumerate(fixtures):
        actual = Flow(mask(source), 'draw' if index == 11 else 'original').policy()
        if actual != expected:
            raise AssertionError((index, expected, actual))
    code = mask('void helper(PPCContext& ctx, uint8_t* b, PPCFunc* draw) { if(a) {draw(ctx,b); return;} draw(ctx,b); }')
    hs = helpers(code)
    policy, position, ev = original_calls('sub_82000000', ' helper(ctx, base, __imp__sub_82000000); ', hs, 0, lambda at: 1)
    assert (policy, position, ev['parameter']) == ('always', 'helper', 'draw')
    print('13 lexical/control-flow/forwarding checks passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--csv', type=Path, default=DEST / 'data/hook-inventory.csv')
    parser.add_argument('--markdown', type=Path, default=DEST / 'hook-inventory.md')
    parser.add_argument('--self-test', action='store_true', help='Test lexical/path/forwarding analysis without reading game data')
    parser.add_argument('--check', action='store_true', help='Fail on count mismatch, missing policy or excessive unclassified hooks')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    paths = sorted(p for p in (ROOT / 'src').glob('*') if p.suffix in ('.cpp', '.h'))
    doc_paths = sorted((ROOT / 'docs').glob('*.md'))
    docs = {p.relative_to(ROOT).as_posix(): p.read_text(encoding='utf-8-sig') for p in doc_paths}
    inputs = paths + doc_paths + [ROOT / 'config/analysis.toml', SDK_HOOK, Path(__file__).resolve()]
    hashes = {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inputs)}
    manifest_hash = hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()
    sdk = SDK_HOOK.read_text(encoding='utf-8')
    macros = sorted(set(re.findall(r'^#define\s+((?:REX_(?:HOOK|STUB|EXPORT)\w*|XBOXKRNL_EXPORT\w*|XAM_EXPORT\w*|REXCRT_EXPORT\w*))\(', sdk, re.M)))
    macro_re = re.compile(r'\b(' + '|'.join(map(re.escape, macros)) + r')\s*\(')
    rows, hook_count, unresolved_docs = [], 0, []
    raw_count = 0
    for path in paths:
        raw, code, line = source_file(path)
        rel = path.relative_to(ROOT).as_posix()
        local_helpers = helpers(code)
        comments = {m[1].upper(): m[2].strip() for m in re.finditer(
            r'(?:__imp__)?sub_([0-9a-fA-F]{8})\s*\)?\s*;?\s*//\s*([^\n]+)', raw)}
        raw_count += len(re.findall(r'\bREX_HOOK_RAW\s*\(', code))
        spans = []

        def emit(kind, at, end=None, **values):
            row = dict.fromkeys(FIELDS, '')
            row.update(kind=kind, file=rel, line=line(at), end_line=line(end-1 if end else at),
                       input_manifest_sha256=manifest_hash)
            row.update(values)
            for key in ('original_evidence', 'offsets', 'evidence'):
                if isinstance(row[key], (dict, list)):
                    row[key] = json.dumps(row[key], ensure_ascii=False, sort_keys=True)
            rows.append(row)

        for m in macro_re.finditer(code):
            ranges, end = arguments(code, code.index('(', m.start()))
            symbol = code[slice(*ranges[0])].strip()
            start = end
            while start < len(code) and code[start].isspace():
                start += 1
            is_raw = m[1] == 'REX_HOOK_RAW'
            body_end = balanced(code, start, '{', '}') if is_raw and code[start:start+1] == '{' else end
            if is_raw and body_end == end:
                raise ValueError('Raw hook without body: %s:%d' % (rel, line(m.start())))
            body = code[start+1:body_end-1] if is_raw else ''
            spans.append((m.start(), body_end, symbol))
            match = SYMBOL.fullmatch(symbol)
            address = '0x' + match[1].upper() if match else ''
            name = comments.get(match[1].upper(), '') if match else symbol
            matches = [p for p, text in docs.items() if symbol in text or (address and address[2:] in text) or path.name in text]
            if not matches:
                unresolved_docs.append(symbol)
            matches.append('docs/native-port/hook-inventory.md')
            policy, position, evidence = original_calls(symbol, body, local_helpers, start+1, line) if is_raw else (
                'never', 'native-or-stub', {'macro': m[1], 'definition': 'research/rexglue-source/include/rex/hook.h'})
            emit('hook', m.start(), body_end, symbol=symbol, guest_address=address,
                 subsystem=subsystem(path, symbol, name), source_name=name, name_confidence='source-comment-unverified',
                 original_policy=policy, original_position=position, original_evidence=evidence,
                 docs=';'.join(matches), expression=raw[m.start():end].strip(),
                 evidence={'macro': m[1], 'source_sha256': hashes[rel], 'existing_document_match': bool(len(matches)-1)})
            hook_count += 1
        for m in SYMBOL.finditer(code):
            tail = code[m.end():]
            before = code[max(0, m.start()-32):m.start()]
            role = 'direct-call' if re.match(r'\s*\(', tail) else 'reference-or-function-pointer'
            if re.search(r'REX_(?:EXTERN|IMPORT)\s*\(\s*$', before):
                role = 'declaration-or-import'
            if any(a <= m.start() < b and m.start() < code.find('{', a, b) for a,b,_ in spans):
                role = 'hook-definition'
            owner = next((s for a,b,s in spans if a <= m.start() < b), '')
            emit('guest-symbol', m.start(), symbol=m.group(), guest_address='0x'+m[1].upper(),
                 subsystem=DEFAULTS.get(path.name, 'support'), expression=role,
                 evidence={'enclosing_hook': owner, 'source_sha256': hashes[rel]})
        for m in ADDRESS.finditer(code):
            emit('guest-address', m.start(), guest_address=m.group().upper().replace('0X', '0x'),
                 subsystem=DEFAULTS.get(path.name, 'support'), expression=raw.splitlines()[line(m.start())-1].strip(),
                 evidence={'source_sha256': hashes[rel]})
        for m in READS.finditer(code):
            at = code.index('(', m.start())
            ranges, end = arguments(code, at)
            # Declarations of the accessors themselves are not memory reads.
            if any(re.search(r'\b(?:uint\d+_t|uint8_t|PPCContext)\b', code[a:b]) for a,b in ranges):
                continue
            expr = raw[m.start():end]
            emit('memory-access', m.start(), end, subsystem=DEFAULTS.get(path.name, 'support'),
                 expression=expr, offsets=re.findall(r'\+\s*(0x[0-9a-fA-F]+|\d+)\b', code[at:end]),
                 evidence={'kind': 'accessor-or-macro', 'source_sha256': hashes[rel]})
        for m in re.finditer(r'\b(?:base|b|guest_base)\s*\[', code):
            at = code.index('[', m.start())
            end = balanced(code, at, '[', ']')
            emit('memory-access', m.start(), end, subsystem=DEFAULTS.get(path.name, 'support'),
                 expression=raw[m.start():end], offsets=re.findall(r'\+\s*(0x[0-9a-fA-F]+|\d+)\b', code[at:end]),
                 evidence={'kind': 'byte-access', 'source_sha256': hashes[rel]})
        for m in re.finditer(r'\b(?:reinterpret_cast\s*<[^;\n]+?>|memcpy|GuestPtr(?:<[^;\n]+?>)?)\s*\(', code):
            at = code.index('(', m.start())
            end = balanced(code, at)
            if not re.search(r'\b(?:base|b|guest_base)\s*\+', code[at:end]):
                continue
            emit('memory-access', m.start(), end, subsystem=DEFAULTS.get(path.name, 'support'),
                 expression=raw[m.start():end], offsets=re.findall(r'\+\s*(0x[0-9a-fA-F]+|\d+)\b', code[at:end]),
                 evidence={'kind': 'pointer-conversion-or-bulk-access', 'source_sha256': hashes[rel]})
        for m in re.finditer(r'\bTranslateVirtual\s*(?:<[^;\n]+?>)?\s*\(', code):
            at = code.index('(', m.start())
            end = balanced(code, at)
            emit('memory-access', m.start(), end, subsystem=DEFAULTS.get(path.name, 'support'),
                 expression=raw[m.start():end], offsets=re.findall(r'\+\s*(0x[0-9a-fA-F]+|\d+)\b', code[at:end]),
                 evidence={'kind': 'guest-memory-translation', 'source_sha256': hashes[rel]})
    cfg_path = ROOT / 'config/analysis.toml'
    cfg = tomllib.loads(cfg_path.read_text(encoding='utf-8-sig'))
    cfg_text = cfg_path.read_text(encoding='utf-8-sig')
    for section, entries in sorted(cfg.items()):
        for address, value in sorted(entries.items()):
            m = re.search(r'^' + re.escape(address) + r'\s*=', cfg_text, re.M)
            row = dict.fromkeys(FIELDS, '')
            row.update(kind='analysis-config', file='config/analysis.toml', line=cfg_text[:m.start()].count('\n')+1,
                       guest_address=address, expression=json.dumps(value, sort_keys=True), subsystem='codegen',
                       input_manifest_sha256=manifest_hash, evidence=json.dumps({'section': section}))
            rows.append(row)
    rows.sort(key=lambda r:(r['file'], int(r['line']), r['kind'], r['symbol'], r['expression']))
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open('w', encoding='utf-8', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS, lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    hooks = [r for r in rows if r['kind']=='hook']
    by_system = Counter(r['subsystem'] for r in hooks)
    by_policy = Counter(r['original_policy'] for r in hooks)
    kinds = Counter(r['kind'] for r in rows)
    if args.check and (sum(json.loads(r['evidence'])['macro']=='REX_HOOK_RAW' for r in hooks) != raw_count
            or any(not r['original_policy'] or not r['subsystem'] for r in hooks)
            or by_system['support'] > len(hooks) * .1):
        raise SystemExit('Hook inventory validation failed')
    lines = ['# Опись вмешательств порта', '',
        'Создано `python -I scripts/inventory_port_hooks.py`. Исходники и поведение игры не меняются.', '',
        'Имена взяты из исходных комментариев и **не проверены**; T01 ещё не принята.',
        'Каждая строка CSV — место использования, а не уникальная функция или уникальное поле.', '',
        f'Всего хуков: **{hook_count}**; `REX_HOOK_RAW`: **{raw_count}**.',
        'Строки по виду: `' + json.dumps(dict(sorted(kinds.items())), sort_keys=True) + '`.',
        'Original policy: `' + json.dumps(dict(sorted(by_policy.items())), sort_keys=True) + '`.', '',
        '## Значение original policy', '',
        '`always` — каждый разобранный путь вызывает оригинал; `conditional` — есть путь без него;',
        '`never` — хук не вызывает рекомпилированный оригинал. Это не означает готовую независимую',
        'нативную подсистему: хук может вызывать другие guest-функции или native API SDK.',
        '`after`/`before`/`around` — расположение вызова относительно остального текста тела;',
        '`helper` — оригинал передан параметром; имя helper и его строка приведены в evidence.',
        'Циклы и switch анализируются консервативно; `flow_overapproximation` перечисляет такие конструкции.', '',
        'Смещения извлечены из выражений доступа. Пустой offsets означает динамическое/именованное',
        'смещение либо прямой адрес. Полные выражения сохранены; это не восстановленная раскладка классов.',
        'Доступы через Read/Write, REX_LOAD/STORE и byte indexing учитываются синтаксически;',
        'динамические цепочки указателей, алиасы функций/указателей требуют отдельного анализа.', '',
        '## Подсистемы', '', '| Подсистема | Хуков | Без оригинала |', '|---|---:|---:|']
    for system, count in sorted(by_system.items()):
        lines.append(f'| {system} | {count} | {sum(r["subsystem"]==system and r["original_policy"]=="never" for r in hooks)} |')
    lines += ['', '## Все хуки', '', '| Подсистема | Символ | Исходник | Original | Позиция | Имя из комментария |', '|---|---|---|---|---|---|']
    for row in hooks:
        name = row['source_name'].replace('|','\\|')
        lines.append(f'| {row["subsystem"]} | `{row["symbol"]}` | `{row["file"]}:{row["line"]}` | {row["original_policy"]} | {row["original_position"]} | {name} |')
    lines += ['', '## Полностью обходящие оригинал', '',
              ', '.join('`'+r['symbol']+'`' for r in hooks if r['original_policy']=='never'), '',
              '## Макросы SDK и связь с документацией', '',
              'Ищутся все определения hook/stub/export и legacy aliases из локального hook.h: ',
              ', '.join('`'+m+'`' for m in macros) + '.', '',
              f'Без точного упоминания символа/адреса или имени файла в существующих docs/*.md: **{len(unresolved_docs)}** хуков.',
              'Для них документированная точка входа — эта опись. Ссылка на документ по имени исходника',
              'не доказывает, что документ описывает каждую ветвь конкретного хука.', '',
              '## SHA-256 входов', '', f'Манифест: `{manifest_hash}`.', '', '| Вход | SHA-256 |', '|---|---|']
    lines += [f'| `{p}` | `{h}` |' for p,h in hashes.items()]
    args.markdown.parent.mkdir(parents=True, exist_ok=True)
    args.markdown.write_text('\n'.join(lines)+'\n', encoding='utf-8')
    print(json.dumps({'hooks':hook_count, 'REX_HOOK_RAW':raw_count, 'by_subsystem':dict(sorted(by_system.items())),
                      'by_original_policy':dict(sorted(by_policy.items())), 'rows':dict(sorted(kinds.items())),
                      'unique_guest_symbols':len({m.group(1).upper() for p in paths for m in SYMBOL.finditer(mask(p.read_text(encoding='utf-8-sig')))}),
                      'input_files':len(hashes), 'input_manifest_sha256':manifest_hash}, indent=2))


if __name__ == '__main__':
    main()
