"""Check behavior and immutable evidence for the expanded mouse feedback."""
import json
import re
from datetime import datetime
from pathlib import Path
from input_audit_manifest import ROOT, sha, source_hashes, verify

data = json.loads((ROOT / 'runtime-data/feedback-regression-runs.json').read_text(encoding='utf-8-sig'))
logs = {name: Path(run['log']).read_text(encoding='utf-8', errors='replace') for name, run in data['runs'].items()}
checks = {}
def check(name, value):
    checks[name] = bool(value)
for name, log in logs.items():
    check(name + '_completed', re.search(r'PvZ (?:frame )?replay completed ', log) and not re.search(
        r'\[FATAL\]|\[critical\]|Unhandled guest|access violation|timed out', log, re.I))
    check(name + '_frames', len(list(Path(data['runs'][name]['frames']).glob('frame-*.ppm'))) >= 8)

def contains(name, text):
    return text in logs.get(name, '')
check('console_beghouled_swipe_matches', bool(re.search(r'PvZ puzzle mode 21 state 2 matches [1-9]', logs.get('begh_console', ''))))
check('console_twist_matches', bool(re.search(r'PvZ puzzle mode 25 state 2 matches [1-9]', logs.get('twist_console', ''))))
for interval in (0, 1):
    check('dxgi_present_policy_' + str(interval), contains('begh_console', f'PvZ DXGI Present policy: new frame interval {interval},'))
check('slot_pc_click_only_seed', logs.get('slot_pc', '').count('PvZ mouse dropped seed picked true') >= 2
      and 'PvZ Slot Machine auto picked dropped seed' not in logs.get('slot_pc', ''))
check('slot_pc_plant', contains('slot_pc', 'PvZ mouse dropped seed placed true'))
check('slot_pc_cancel', contains('slot_pc', 'PvZ mouse cancel seed'))
check('slot_pc_cancel_keeps_hand_empty', 'PvZ Slot Machine auto picked dropped seed' not in logs.get('slot_pc', '').rsplit('PvZ mouse cancel seed', 1)[-1])
check('stars_select_and_plant', contains('stars_pc', 'PvZ mouse pick packet 0 allowed true') and contains('stars_pc', 'PvZ mouse planted seed'))
check('izombie_select_and_place', contains('izombie_pc', 'PvZ mouse pick packet 0 allowed true') and contains('izombie_pc', 'PvZ mouse planted seed'))
for name in ('laststand_pc', 'laststand_console'):
    check(name + '_onslaught', contains(name, 'PvZ mouse Start Onslaught activated'))
check('scrollbar_drag', logs.get('scrollbar', '').count('PvZ challenge scrollbar page ') >= 2)
check('scrollbar_back', contains('scrollbar', 'PvZ dialog activate button 1101') and contains('scrollbar', 'PvZ service back vtable 820B899C'))
check('quarium_pc_buy', bool(re.search(r'minigame 24 buy packet 0 .*sun 1500 -> 1400', logs.get('quarium_pc', ''))))
check('quarium_pc_feed', 'sun 1395' in logs.get('quarium_pc', ''))
for name in ('whack_pc', 'whack_console'):
    check(name + '_plant_cost', 'sun 975' in logs.get(name, ''))
buys = re.findall(r'Heavy Weapon buy packet \d+ sun (\d+) -> (\d+)', logs.get('heavy_console', ''))
check('heavy_console_buy', any(int(before) > int(after) for before, after in buys))
check('heavy_console_release', contains('heavy_console', 'PvZ Heavy Weapon LMB released'))
check('vases_pc_break', bool(re.search(r'PvZ Vasebreaker click mode 52 handled [1-9]', logs.get('vases_pc', ''))))
check('vases_pc_explicit_seed', contains('vases_pc', 'PvZ mouse dropped seed picked true'))
check('vases_pc_place_seed', contains('vases_pc', 'PvZ mouse dropped seed placed true'))
steps = re.findall(r'console shovel released after \S+ dug (true|false) tutorial (\d+) -> (\d+)', logs.get('shovel_console', ''))
check('shovel_three_held_digs', sum(step[0] == 'true' for step in steps) == 3)
check('shovel_tutorial_complete', any(step[0] == 'true' and step[2] == '18' for step in steps))
check('shovel_game_ready', contains('shovel_console', 'PvZ replay wait_board ready'))
check('windows_name_loaded', any('PvZ local player name ' in log for log in logs.values()))

baseline = Path(data['profile_baseline'])
data.update(checked_at=datetime.now().astimezone().isoformat(), source_sha256=source_hashes(),
    profile_sha256={p.relative_to(baseline).as_posix(): sha(p) for p in baseline.rglob('*') if p.is_file()},
    checks=checks, passed=sum(checks.values()), total=len(checks),
    frame_sha256={str(p): sha(p) for run in data['runs'].values() for p in Path(run['frames']).glob('frame-*.ppm')},
    limitations='Replay verifies selected actions. Physical controllers, rumble, audible sounds, display tearing and complete level playthroughs require manual play testing.')
data['evidence_errors'] = verify(data)
for name, run in data['runs'].items():
    if 'profile_baseline' in run:
        prepared = Path(run['profile_baseline'])
        actual = {p.relative_to(prepared).as_posix(): sha(p) for p in prepared.rglob('*') if p.is_file()}
        if actual != run.get('profile_sha256'):
            data['evidence_errors'].append(name + ': per-case baseline changed')
(ROOT / 'runtime-data/feedback-regression-validation.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
print(json.dumps({'passed': data['passed'], 'total': data['total'], 'failed': [k for k,v in checks.items() if not v], 'evidence_errors': data['evidence_errors']}, indent=2))
raise SystemExit(bool(data['evidence_errors']) or not all(checks.values()))
