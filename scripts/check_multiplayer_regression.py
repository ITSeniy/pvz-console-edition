"""Validate local player assignment and native transitions into multiplayer boards."""
import json
import re
from datetime import datetime
from pathlib import Path
from input_audit_manifest import ROOT, sha, source_hashes, verify

data = json.loads((ROOT / 'runtime-data/multiplayer-regression-runs.json').read_text(encoding='utf-8-sig'))
checks = {}
for name in ('vs_mouse_pad', 'vs_pad_mouse', 'vs_two_pads', 'coop_mouse_pad', 'coop_pad_mouse', 'coop_two_pads'):
    run = data['runs'].get(name, {})
    log = Path(run['log']).read_text(encoding='utf-8', errors='replace') if run else ''
    checks[name + '_completed'] = 'PvZ replay completed ' in log and not re.search(
        r'Unhandled guest|access violation|\[FATAL\]|\[critical\]|timed out', log, re.I)
    checks[name + '_frames'] = bool(run) and len(list(Path(run['frames']).glob('frame-*.ppm'))) >= 10
    first = '18446744073709551615' if name.endswith('mouse_pad') else '88331812753485'
    checks[name + '_first_device_is_player_one'] = f'PvZ local players started: player 1 device {first}' in log
    if name == 'vs_pad_mouse':
        checks['mouse_joins_player_two'] = 'PvZ local mouse joined player 2' in log
        checks['mouse_player_two_ready'] = 'service navigation vtable 820CC2EC action 6 user 1' in log
    mode = '76' if name.startswith('vs') else '(?:7[7-9]|8[0-9])'
    checks[name + '_native_board'] = bool(re.search(rf'PvZ local board mode {mode} scene 3 users 0,1', log))
    if name.startswith('coop'):
        checks[name + '_both_banks_selected'] = 'PvZ local chooser selected 8 banks 4,4 ready 1' in log
    if name == 'vs_mouse_pad': checks['mouse_player_one_plants'] = 'PvZ mouse planted seed' in log
    if name == 'vs_pad_mouse': checks['mouse_player_two_places'] = 'PvZ mouse planted seed' in log

baseline = Path(data['profile_baseline'])
data.update(checked_at=datetime.now().astimezone().isoformat(), source_sha256=source_hashes(),
    profile_sha256={p.relative_to(baseline).as_posix(): sha(p) for p in baseline.rglob('*') if p.is_file()},
    frame_sha256={str(p): sha(p) for run in data['runs'].values() for p in Path(run['frames']).glob('frame-*.ppm')},
    checks=checks, passed=sum(checks.values()), total=len(checks),
    limitations='Two diagnostic physical pad sources and mouse input. Real controllers, rumble and full matches require manual testing.')
data['evidence_errors'] = verify(data)
(ROOT / 'runtime-data/multiplayer-regression-validation.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
print(json.dumps({'passed': data['passed'], 'total': data['total'], 'failed': [k for k,v in checks.items() if not v], 'evidence_errors': data['evidence_errors']}, indent=2))
raise SystemExit(bool(data['evidence_errors']) or not all(checks.values()))
