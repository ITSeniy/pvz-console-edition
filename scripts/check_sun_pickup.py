"""Check natural sun collection and held-mouse regression in native Adventure."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import re
import sys

if len(sys.argv) != 3:
    sys.exit('Usage: check_sun_pickup.py <pickup.log> <hold.log>')
paths = {k: Path(p) for k, p in zip(('pickup', 'hold'), sys.argv[1:])}
logs = {k: p.read_text(encoding='utf-8') for k, p in paths.items()}
checks = {}
for name, log in logs.items():
    checks[name + '_native_adventure_pc'] = all(s in log for s in (
        'Direct3D 12 device and OS features', 'cursor mode: pc', 'mouse state adventure true mode 0')) and not any(
        s in log.lower() for s in ('[fatal]', '[critical]', 'access violation', 'exception', 'timed out'))
    checks[name + '_natural_sun_no_fixture'] = 'replay fixture' not in log
    checks[name + '_click_completes_collection'] = bool(re.search(
        r'clicked coin pickup completed type 4 state 7 picked true', log))
    amounts = [int(v) for v in re.findall(r'PvZ controls [^\n]* sun (\d+) cursor', log)]
    checks[name + '_sun_counter_increases_25'] = any(b - a == 25 for a, b in zip(amounts, amounts[1:]))
    checks[name + '_no_console_autocollect'] = 'console auto collect coin' not in log

pickup = logs['pickup']
checks['multiple_individual_suns_complete'] = pickup.count('clicked coin pickup completed type 4 state 7 picked true') >= 2
hold = logs['hold']
down = hold.find('PvZ replay left_down (700, 500)')
up = hold.find('PvZ replay left_up (625, 383.333)', down)
held = hold[down:up] if down >= 0 and up > down else ''
checks['held_lmb_no_sun_magnet'] = bool(held) and 'mouse collect coin type 4' not in held and 'clicked coin pickup completed' not in held
amounts = [int(v) for v in re.findall(r'PvZ controls [^\n]* sun (\d+) cursor', held)]
checks['held_lmb_sun_counter_unchanged'] = len(amounts) >= 8 and len(set(amounts)) == 1
checks['separate_click_after_release_collects'] = up > down >= 0 and 'mouse collect coin type 4' in hold[up:] and (
    'clicked coin pickup completed type 4 state 7 picked true' in hold[up:])

result = {
    'checked_at': datetime.now().astimezone().isoformat(),
    'built_executable_sha256': hashlib.sha256(Path('build/pvz-console-edition.exe').read_bytes()).hexdigest(),
    'passed': sum(checks.values()), 'total': len(checks), 'checks': checks,
    'native_logs': {k: {'path': str(p.resolve()), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for k, p in paths.items()},
    'scope': 'Native fresh-profile Adventure, naturally spawned sun, individual mouse clicks, delayed credit, held LMB sweep without collection and collection after release. No guest state or sun spawn fixtures.'
}
Path('runtime-data/sun-pickup-validation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
if result['passed'] != result['total']:
    sys.exit(1)
