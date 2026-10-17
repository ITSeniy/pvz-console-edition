#!/usr/bin/env python3
"""Convert seconds CSV using recorded delivery steps, never an assumed FPS.

Inputs: original replay and logs captured with --pvz_replay_record_steps=true.
Output: new frame CSV plus JSON SHA-256 manifest. Source files are read only.
Rows are numbered after removing blank/comment lines, exactly as input_replay.cpp.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def convert(source, logs, output):
    rows = [row for row in source.read_text(encoding='utf-8-sig').splitlines()
            if row.strip() and not row.lstrip().startswith('#')]
    deliveries = {}
    for log in logs:
        for index, step in re.findall(r'PvZ replay recorded row (\d+) step (\d+)',
                                      log.read_text(encoding='utf-8', errors='replace')):
            index, step = int(index), int(step)
            if index in deliveries:
                raise ValueError(f'duplicate recorded row {index}')
            deliveries[index] = step
    if sorted(deliveries) != list(range(len(rows))):
        raise ValueError('recorded rows must cover the entire source exactly once')
    previous = -1
    converted = []
    for index, row in enumerate(rows):
        fields = next(csv.reader([row]))
        if len(fields) != 4:
            raise ValueError(f'invalid source row {index}')
        step = deliveries[index]
        if step < previous:
            raise ValueError('recorded steps are not monotonic')
        previous = step
        converted.append(','.join([str(step), *fields[1:]]))
    if output.exists() or output.with_suffix(output.suffix + '.json').exists():
        raise ValueError('outputs must be new; existing evidence is preserved')
    manifest = {'schema': 1, 'source': str(source.resolve()), 'source_sha256': digest(source),
                'logs': {str(p.resolve()): digest(p) for p in logs},
                'converter_sha256': digest(Path(__file__)), 'rows': len(rows),
                'method': 'recorded app logical update delivery; no FPS interpolation'}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text('# pvz-frame-locked-v1\n' + '\n'.join(converted) + '\n', encoding='utf-8')
    manifest['output_sha256'] = digest(output)
    output.with_suffix(output.suffix + '.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(manifest, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('logs', nargs='+', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        convert(args.source, args.logs, args.output)
    except (ValueError, OSError) as error:
        parser.exit(2, f'{error}\n')


if __name__ == '__main__':
    main()
