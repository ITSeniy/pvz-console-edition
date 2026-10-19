"""Extract verbatim generated leaf bodies into the build directory, never inputs.

Inputs: B001 declarations, generated/default/*.cpp, Xbox image. Output: isolated
test TU and SHA-256 manifest. Method: balanced braces, no translated reference.
"""
import hashlib
import json
from pathlib import Path
import re
import sys

root, output = map(Path, sys.argv[1:])
source = root / 'src/native/b001_widget.cpp'
selected = re.findall(r'^LEAF\(([0-9A-F]{8}), .*?, (Identity|FocusByte|Digits|Color)\)$', source.read_text(), re.M)
names = {address for address, _ in selected}
hashes = {}
bodies = {}
for path in sorted((root / 'generated/default').glob('pvz_recomp.*.cpp')):
    text = path.read_text()
    for match in re.finditer(r'DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\) \{', text):
        if match[1] not in names:
            continue
        start, end, depth = match.start(), match.end(), 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        bodies[match[1]] = text[start:end]
        hashes[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
if bodies.keys() != names:
    raise ValueError(f'missing generated bodies: {names - bodies.keys()}')
for path in (source, root / '.cache/pvz-image.bin', Path(__file__),
             root / 'src/native/replacement.h', root / 'src/xbox/Widget.h',
             root / 'tests/b001_synthetic_test.cpp'):
    hashes[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
output.write_text('#include "pvz_pch.h"\n' + '\n\n'.join(bodies[a] for a in sorted(bodies)))
output.with_suffix('.json').write_text(json.dumps({'sha256': hashes, 'functions': sorted(names)}, indent=2))
