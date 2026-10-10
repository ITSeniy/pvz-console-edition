"""Create a verified executable overlay for the previous tester build."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import sys
import zipfile

root = Path(__file__).resolve().parents[1]
baseline = root / 'dist/PVZXboxRecomp-test-20261008-151815.zip'
def digest(data):
    return hashlib.sha256(data).hexdigest()
exe = (root / 'build/pvz.exe').read_bytes()
report = json.loads((root / 'runtime-data/sun-pickup-validation.json').read_text())
if report['passed'] != report['total'] or report['built_executable_sha256'] != digest(exe):
    sys.exit('Sun pickup validation must pass on the current executable')
with zipfile.ZipFile(baseline) as z:
    prefix = next(n for n in z.namelist() if n.endswith('/build-info.json')).removesuffix('build-info.json')
    manifest = json.loads(z.read(prefix + 'build-info.json').decode('utf-8-sig'))
    for path in ('build/rexruntimerd.dll', 'build/rexgpu-xenosrd.dll', 'build/TracyClientrd.dll',
                 'build/pc_defaults.toml', 'run.ps1', 'config/game.lock.json', 'assets/main.pak', 'assets/en.pak'):
        if z.read(prefix + path) != (root / path).read_bytes():
            sys.exit(f'Baseline dependency changed: {path}; create a full build instead')
    baseline_exe_sha = digest(z.read(prefix + 'build/pvz.exe'))
files = {
    'build/pvz.exe': exe,
    'docs/sun-pickup-fix.md': (root / 'docs/sun-pickup-fix.md').read_bytes(),
    'validation/sun-pickup-validation.json': (root / 'runtime-data/sun-pickup-validation.json').read_bytes(),
    'SUN-FIX-README.md': (root / 'docs/sun-pickup-fix.md').read_bytes()
}
info = {
    'kind': 'executable-overlay', 'created_at': datetime.now().astimezone().isoformat(),
    'baseline_build': manifest['build'], 'baseline_archive_sha256': digest(baseline.read_bytes()),
    'baseline_executable_sha256': baseline_exe_sha, 'fixed_executable_sha256': digest(exe),
    'validation_passed': report['passed'], 'validation_total': report['total'],
    'includes_user_profile': False,
    'files': [{'path': name, 'bytes': len(data), 'sha256': digest(data)} for name, data in files.items()]
}
files['sun-fix-info.json'] = json.dumps(info, indent=2).encode('utf-8')
archive = root / f'dist/PVZXboxRecomp-sun-fix-{datetime.now():%Y%m%d-%H%M%S}.zip'
with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as z:
    for name, data in files.items():
        z.writestr(name, data)
with zipfile.ZipFile(archive) as z:
    assert set(z.namelist()) == set(files)
    for name, data in files.items():
        assert z.read(name) == data, name
sha = digest(archive.read_bytes())
archive.with_suffix('.zip.sha256').write_text(f'{sha}  {archive.name}\n', encoding='ascii')
print(json.dumps({'archive': str(archive), 'bytes': archive.stat().st_size,
                  'sha256': sha, 'files_verified': len(files), 'baseline_build': manifest['build']}, indent=2))
