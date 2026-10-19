"""T15 Xbox evidence: inputs are the image, class/name CSVs and generated C++.

Write per-function disassembly, verbatim generated bodies and SHA-256 manifest
to an explicitly selected output directory. Never modifies the game inputs.
"""
import csv
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools/python'))
from capstone import Cs, CS_ARCH_PPC, CS_MODE_32, CS_MODE_BIG_ENDIAN

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
sha = {}
def read(path):
    data = path.read_bytes()
    sha[path.relative_to(root).as_posix()] = hashlib.sha256(data).hexdigest()
    return data

classes = list(csv.DictReader(read(root / 'docs/native-port/data/xbox-function-classes.csv').decode('utf-8-sig').splitlines()))
names = {int(r['xbox_address'], 16): r['name'] for r in csv.DictReader(read(root / 'docs/native-port/data/xbox-names.csv').decode('utf-8-sig').splitlines())}
selected = {r['address'][2:]: r for r in classes if r['batch'] in ('B002', 'B003')}
image = read(root / '.cache/pvz-image.bin')
dis = Cs(CS_ARCH_PPC, CS_MODE_32 | CS_MODE_BIG_ENDIAN)
for path in sorted((root / 'generated/default').glob('pvz_recomp.*.cpp')):
    text = path.read_text()
    for m in re.finditer(r'DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\) \{', text):
        if m[1] not in selected:
            continue
        read(path)
        end, depth = m.end(), 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        row = selected[m[1]]
        address = int(m[1], 16)
        body = text[m.start():end]
        code = image[address-0x82000000:address-0x82000000+int(row['bytes'])]
        ins = [f'{i.address:08X} {i.mnemonic} {i.op_str}' for i in dis.disasm(code, address)]
        row.update(name=names.get(address, row['class']+'_'+m[1]), generated=path.relative_to(root).as_posix(),
                   children=sorted(set(re.findall(r'(?:sub_[0-9A-F]{8}|REX_CALL_INDIRECT_FUNC)', body))), instructions='; '.join(ins))
        (out / (m[1]+'.cpp')).write_text(body, encoding='utf-8')
        (out / (m[1]+'.txt')).write_text(f"{row['batch']} {row['name']} {row['bytes']} bytes\n"+'\n'.join(ins), encoding='utf-8')
read(Path(__file__).resolve())
(out / 'manifest.json').write_text(json.dumps({'sha256': sha, 'functions': selected}, indent=2), encoding='utf-8')
for r in selected.values():
    print(r['batch'], r['address'], r['bytes'], r['name'], '|'.join(r['children']))
