"""Strict, streaming comparison of T05 JSON Lines game states.

Inputs: two complete JSONL files, read only. Output: first differing field/frame,
counts and SHA-256 inputs, optionally JSON (--output). Exit 0 equal, 1 different,
2 malformed/empty/incompatible capture. No numeric tolerance or frame alignment.
Metadata is checked separately; all fields of each state participate in equality.
"""
import argparse
from collections import Counter
import hashlib
from itertools import zip_longest
import json
from pathlib import Path
import tempfile

METADATA = ('schema', 'phase', 'hash', 'rng', 'xex_sha256')
ORDER = ['frame', 'mode', 'scene', 'root_vtable', 'board', 'paused', 'sun', 'sun_p2',
         'plants', 'zombies', 'projectiles', 'coins', 'rng_index', 'rng_hash',
         'valid', 'count', 'allocated', 'used', 'next_key', 'ids', 'type', 'x', 'y', 'health', 'state', 'hash']


def checksum(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def unique_object(items):
    result = {}
    for key, value in items:
        if key in result:
            raise ValueError('duplicate JSON key: ' + key)
        result[key] = value
    return result


class Capture:
    def __init__(self, path):
        self.path = Path(path)
        self.metadata = None
        self.frames = 0
        self.board_frames = 0
        self.modes = Counter()
        self.playing_modes = Counter()
        self.roots = Counter()
        self.peak_counts = Counter()
        self.final_frame = None

    def rows(self):
        with self.path.open(encoding='utf-8') as stream:
            for number, line in enumerate(stream, 1):
                try:
                    if not line.endswith('\n'):
                        raise ValueError('incomplete final line')
                    row = json.loads(line, object_pairs_hook=unique_object,
                                     parse_constant=lambda s: (_ for _ in ()).throw(ValueError('invalid JSON constant: ' + s)))
                    if not isinstance(row, dict):
                        raise ValueError('record must be an object')
                    if number == 1:
                        if row.get('record') != 'metadata' or type(row.get('schema')) is not int or row['schema'] != 1:
                            raise ValueError('expected schema 1 metadata first')
                        if any(not isinstance(row.get(key), str) or not row[key] for key in ('phase','hash','rng')):
                            raise ValueError('missing capture phase/hash/RNG metadata')
                        self.metadata = row
                        continue
                    if row.get('record') != 'state' or type(row.get('frame')) is not int or row['frame'] != self.frames:
                        raise ValueError('expected consecutive state frame ' + str(self.frames))
                    for key in ('mode', 'scene', 'root_vtable', 'sun', 'sun_p2', 'rng_index'):
                        if type(row.get(key)) is not int:
                            raise ValueError('missing/integer field: ' + key)
                    for key in ('board', 'paused'):
                        if type(row.get(key)) is not bool:
                            raise ValueError('missing/boolean field: ' + key)
                    if not isinstance(row.get('rng_hash'), str) or len(row['rng_hash']) != 16:
                        raise ValueError('missing RNG digest')
                    for name in ('plants', 'zombies', 'projectiles', 'coins'):
                        pool = row.get(name)
                        if not row['board']:
                            if pool is not None or name not in row:
                                raise ValueError('menu pool must be present and null: ' + name)
                            continue
                        if not isinstance(pool, dict) or pool.get('valid') is not True:
                            raise ValueError('invalid or missing pool: ' + name)
                        for field in ('count', 'allocated', 'used', 'next_key'):
                            if type(pool.get(field)) is not int or pool[field] < 0:
                                raise ValueError('missing pool integer: ' + name + '.' + field)
                        if not pool['count'] <= pool['allocated'] <= pool['used'] <= 65536:
                            raise ValueError('inconsistent pool counts: ' + name)
                        for field in ('hash', 'ids', 'type', 'x', 'y', 'health', 'state'):
                            if not isinstance(pool.get(field), str) or len(pool[field]) != 16:
                                raise ValueError('missing pool digest: ' + name + '.' + field)
                        self.peak_counts[name] = max(self.peak_counts[name], pool['count'])
                    self.frames += 1
                    self.final_frame = row['frame']
                    self.board_frames += row['board']
                    self.modes[row['mode']] += 1
                    self.roots[row['root_vtable']] += 1
                    if row['board'] and row['scene'] == 3:
                        self.playing_modes[row['mode']] += 1
                    yield row
                except (ValueError, TypeError, KeyError) as error:
                    raise ValueError(f'{self.path}:{number}: {error}') from error
        if not self.frames:
            raise ValueError(str(self.path) + ': no state frames')

    def summary(self):
        return {'frames': self.frames, 'board_frames': self.board_frames, 'modes': dict(self.modes),
                'playing_modes': dict(self.playing_modes), 'root_vtables': dict(self.roots),
                'peak_counts': dict(self.peak_counts), 'last_frame': self.final_frame}


def differences(left, right, path=''):
    if type(left) is not type(right):
        yield path, left, right
    elif isinstance(left, dict):
        keys = sorted(left.keys() | right.keys(), key=lambda k: (ORDER.index(k) if k in ORDER else len(ORDER), k))
        for key in keys:
            here = path + '.' + key if path else key
            if key not in left or key not in right:
                yield here, left.get(key, '<missing>'), right.get(key, '<missing>')
            else:
                yield from differences(left[key], right[key], here)
    elif isinstance(left, list):
        for index, (a, b) in enumerate(zip_longest(left, right, fillvalue='<missing>')):
            yield from differences(a, b, f'{path}[{index}]')
    elif left != right:
        yield path, left, right


def compare(a, b):
    left, right = Capture(a), Capture(b)
    first = None
    differing_frames = 0
    fields = Counter()
    for ra, rb in zip_longest(left.rows(), right.rows()):
        if ra is None or rb is None:
            changes = [('capture.length', ra['frame'] if ra else '<end>', rb['frame'] if rb else '<end>')]
        else:
            changes = list(differences(ra, rb))
        if changes:
            differing_frames += 1
            for field, av, bv in changes:
                fields[field] += 1
                if first is None:
                    first = {'frame': (ra if ra is not None else rb)['frame'], 'field': field, 'a': av, 'b': bv}
    for key in METADATA:
        if left.metadata.get(key) != right.metadata.get(key):
            raise ValueError('incompatible metadata: ' + key)
    return {'equal': first is None, 'first_difference': first, 'differing_frames': differing_frames,
            'differing_fields': dict(fields), 'a': {'path': str(a), 'sha256': checksum(Path(a)), **left.summary()},
            'b': {'path': str(b), 'sha256': checksum(Path(b)), **right.summary()}}


def self_test():
    header = {'record':'metadata', 'schema':1, 'phase':'after-root-update', 'hash':'test', 'rng':'test'}
    row = {'record':'state', 'frame':0, 'mode':0, 'scene':1, 'root_vtable':1, 'board':False,
           'paused':False, 'sun':0, 'sun_p2':0, 'rng_index':0, 'rng_hash':'0'*16,
           'plants':None, 'zombies':None, 'projectiles':None, 'coins':None}
    with tempfile.TemporaryDirectory(prefix='pvz-state-compare-') as directory:
        a, b = Path(directory)/'a.jsonl', Path(directory)/'b.jsonl'
        def write(path, rows):
            path.write_text(''.join(json.dumps(r)+'\n' for r in [header]+rows), encoding='utf-8')
        write(a, [row]); write(b, [row]); assert compare(a,b)['equal']
        changed = {**row, 'sun':1}; write(b,[changed])
        assert compare(a,b)['first_difference'] == {'frame':0,'field':'sun','a':0,'b':1}
        write(b,[row,{**row,'frame':1}]); assert compare(a,b)['first_difference']['field']=='capture.length'
        for bad in ([{**row,'frame':2}], [{k:v for k,v in row.items() if k!='sun'}],
                    [{**row,'board':1}], [{**row,'board':True}], []):
            write(b,bad)
            try: compare(a,b)
            except ValueError: pass
            else: raise AssertionError('accepted malformed capture')
        b.write_text(a.read_text(encoding='utf-8').rstrip(), encoding='utf-8')
        try: compare(a,b)
        except ValueError: pass
        else: raise AssertionError('accepted truncated line')
        assert list(differences({'x':1},{'x':True}))
    print('Comparator: equality, sensitivity, length, schema, gaps, types and truncation checks passed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('a', type=Path, nargs='?')
    parser.add_argument('b', type=Path, nargs='?')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test(); return 0
    if args.a is None or args.b is None:
        parser.error('two dump paths are required')
    try:
        result = compare(args.a, args.b)
        status = 0 if result['equal'] else 1
    except (ValueError, OSError) as error:
        result, status = {'equal':False, 'error':str(error), 'inputs':{}}, 2
        for path in (args.a, args.b):
            if path.is_file(): result['inputs'][str(path)] = checksum(path)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=True, indent=2))
    return status


if __name__ == '__main__':
    raise SystemExit(main())
