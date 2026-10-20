"""Extract verbatim Xbox function bodies and input SHA-256s for batch synthetics.

Inputs: batch source, generated/default, Xbox image, layouts and test harness.
Output: build-only C++ reference TU and JSON provenance. No body rewriting.
Native test bodies use the common PVZ_BATCH_SYNTHETIC_TEST guard, supplied by
batch_synthetic.h; this extractor does not define a task-specific guard.
"""
import hashlib
import json
from pathlib import Path
import re
import sys
root, output = map(Path, sys.argv[1:3])
batch = sys.argv[3]
source = root / ('src/native/'+{
    'b002':'b002_gamepad.cpp',
    'b003':'b003_widget_container.cpp',
    'b005':'b005_scrollbar.cpp',
    'b010':'b010_data.cpp',
    'b013':'b013_widget_manager.cpp',
    'b017':'b017_bigint.cpp',
}[batch])
source_text = source.read_text()
names = set(re.findall(r'^LEAF\(([0-9A-F]{8}),', source_text, re.M))
if batch == 'b005':
    # Two SnapshotOnly setters are absent from the supplied game replays. Their
    # synthetic tests compare declared guest-memory snapshots against the Xbox
    # bodies, including a deliberately corrupted negative case.
    names.update(re.findall(r'^SNAP\(([0-9A-F]{8}), true, (?:SetHorizontal|SetButtonFields)\)',
                            source_text, re.M))
hashes, bodies = {}, {}
def record(p):
    hashes[p.relative_to(root).as_posix()] = hashlib.sha256(p.read_bytes()).hexdigest()
for path in sorted((root / 'generated/default').glob('pvz_recomp.*.cpp')):
    text = path.read_text()
    for m in re.finditer(r'DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\) \{', text):
        if m[1] not in names:
            continue
        end, depth = m.end(), 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        bodies[m[1]] = text[m.start():end]
        record(path)
if bodies.keys() != names:
    raise ValueError(f'missing {names-bodies.keys()}')
for p in [source, Path(__file__), root/'.cache/pvz-image.bin', root/'src/native/replacement.h',
          root/'tests/batch_synthetic.h', root/f'tests/{batch}_synthetic_test.cpp',
          *sorted((root/'src/xbox').glob('*.h'))]:
    record(p)
output.write_text('#include "pvz_pch.h"\n'+'\n\n'.join(bodies[a] for a in sorted(names)), encoding='utf-8')
output.with_suffix('.json').write_text(json.dumps({'sha256':hashes,'functions':sorted(names)},indent=2),encoding='utf-8')
