"""Fingerprint and verify mouse audit evidence against its executable and sources."""
from pathlib import Path
import argparse
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def source_hashes(root=ROOT):
    paths = [root / 'CMakeLists.txt']
    paths += sorted(p for p in (root / 'src').glob('*') if p.suffix in ('.cpp', '.h'))
    paths += sorted(p for p in (root / 'tests').glob('*') if p.suffix in ('.cpp', '.py'))
    paths += [root / 'scripts' / name for name in
              ('run_input_audit.ps1', 'check_input_coverage.py', 'input_audit_manifest.py')]
    return {p.relative_to(root).as_posix(): sha(p) for p in paths}

def verify(data, root=ROOT):
    errors = []
    if data.get('built_executable_sha256') != sha(root / 'build/pvz-console-edition.exe'):
        errors.append('Executable differs from the tested build')
    if data.get('source_sha256') != source_hashes(root):
        errors.append('Sources or audit scripts differ from the tested snapshot')
    evidence = list(data.get('runs', {}).items())
    evidence += [(f"repeat-{run['repeat']}-{run['name']}", run) for run in data.get('attempts', [])]
    for name, run in evidence:
        for field, key in (('log', 'log_sha256'), ('replay', 'replay_sha256')):
            path = Path(run[field])
            if not path.is_absolute(): path = root / path
            if not path.exists() or run.get(key) != sha(path):
                errors.append(f'{name}: {field} evidence changed or lacks a fingerprint')
    baseline = data.get('profile_baseline')
    if not baseline or not data.get('profile_sha256'):
        errors.append('Missing immutable test-profile baseline')
    else:
        baseline = Path(baseline)
        actual = {p.relative_to(baseline).as_posix(): sha(p) for p in baseline.rglob('*') if p.is_file()}
        if actual != data['profile_sha256']:
            errors.append('Test-profile baseline changed')
    return errors

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path)
    parser.add_argument('--verify', type=Path)
    args = parser.parse_args()
    if args.snapshot:
        args.snapshot.write_text(json.dumps(source_hashes(), indent=2) + '\n', encoding='utf-8')
    elif args.verify:
        errors = verify(json.loads(args.verify.read_text(encoding='utf-8-sig')))
        print(json.dumps({'current': not errors, 'errors': errors}, indent=2))
        raise SystemExit(bool(errors))
    else:
        parser.error('Use --snapshot or --verify')
