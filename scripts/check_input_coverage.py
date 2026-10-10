"""Validate native input-audit runs; keep untested screens explicitly pending."""
from datetime import datetime
from pathlib import Path
import hashlib
import json
import re
import sys
from input_audit_manifest import source_hashes, verify

root = Path(__file__).resolve().parents[1]
def read_json(path):
    return json.loads((root / path).read_text(encoding='utf-8-sig'))
def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

runs = read_json('runtime-data/input-audit-runs.json')
errors = verify(runs)
if errors:
    sys.exit('Stale input audit: ' + '; '.join(errors))
if not runs.get('completed') or set(runs['runs']) != {'tree', 'mail', 'vases', 'almanac', 'izombie'}:
    sys.exit('Complete scripts/run_input_audit.ps1 first')
exe_sha = sha(root / 'build/pvz-console-edition.exe')
if runs['built_executable_sha256'] != exe_sha:
    sys.exit('Audit runs belong to a different executable')
inventory = read_json('research/input-coverage/handlers.json')
families = read_json('research/input-coverage/android-families.json')
logs = {k: Path(v['log']).read_text(encoding='utf-8') for k, v in runs['runs'].items()}
def validate(logs):
    checks = {}
    for name, log in logs.items():
        checks[name + '_native_run'] = 'Direct3D 12 device and OS features' in log and not any(
            bad in log.lower() for bad in ('[fatal]', '[critical]', 'access violation', 'exception', 'timed out', 'invalid input replay'))
        checks[name + '_pc_mode'] = 'PvZ cursor mode: pc' in log
    
    t = logs['tree']
    food = re.findall(r'Tree food click hit 18 balance (\d+) -> (\d+)', t)
    checks['tree_two_native_food_uses'] = food == [('1005', '1004'), ('1004', '1003')]
    checks['tree_empty_click_does_not_feed'] = t.count('replay left_down (400, 350)') == 3 and len(food) == 2
    checks['tree_cancel_and_native_wheel'] = 'Zen tool cancelled' in t and bool(re.search(r'Tree scroll tool -1 -> \d+', t))
    checks['tree_shop_round_trip'] = all(x in t for x in ('input screen StoreScreen', 'mouse Shop back', 'Zen Shop returned'))
    checks['tree_native_garden_switch'] = 'PvZ garden switch tool 17' in t
    
    m = logs['mail']
    checks['mail_native_both_page_commands'] = all(x in m for x in (
        'service navigation vtable 820C21A4 action 2', 'service navigation vtable 820C21A4 action 3'))
    checks['mail_wheel_command'] = 'service wheel vtable 820C21A4 steps -1' in m
    back = m.find('PvZ service back vtable 820C21A4')
    checks['mail_close_to_zen'] = back >= 0 and 'input screen Board' in m[back:] and 'menu activate button 17' in m[back:]
    
    v = logs['vases']
    checks['vases_multiple_native_breaks'] = v.count('Vasebreaker click mode 52 handled 1') >= 3
    auto_pickups = re.findall(r'dropped packet pickup caller 8216EFAC picked (\d+) seed', v)
    checks['vases_native_auto_pickup_blocked'] = bool(auto_pickups) and set(auto_pickups) == {'0'}
    
    a = logs['almanac']
    checks['almanac_both_sections_and_species'] = all(x in a for x in ('Almanac opened page 1', 'Almanac opened page 2')) and (
        len(set(re.findall(r'Almanac selection page 1 plant (\d+)', a))) >= 2 and
        len(set(re.findall(r'Almanac selection page 2 plant \d+ zombie (\d+)', a))) >= 2)
    checks['almanac_native_wheel_both_directions'] = all(x in a for x in (
        'Almanac description wheel -1 position', 'Almanac description wheel 1 position'))
    checks['almanac_three_back_transitions'] = a.count('mouse Almanac back') >= 3
    
    i = logs['izombie']
    checks['izombie_pc_alternate_bank'] = 'mouse state adventure true mode 53 alternate 1' in i
    checks['izombie_two_separate_pick_and_plant'] = i.count('mouse pick packet 0 allowed true') == 2 and i.count('mouse planted seed') == 2
    checks['izombie_empty_click_no_purchase'] = 'replay left_down (550, 295)' in i and i.count('mouse planted seed') == 2
    checks['izombie_native_cost'] = all(f'sun {amount} cursor' in i for amount in (150, 100, 50))
    return checks

checks = {}
repetitions = sorted(set(run['repeat'] for run in runs['attempts']))
for repeat in repetitions:
    group = {run['name']: Path(run['log']).read_text(encoding='utf-8')
             for run in runs['attempts'] if run['repeat'] == repeat}
    if set(group) != {'tree', 'mail', 'vases', 'almanac', 'izombie'}:
        sys.exit(f'Incomplete native repeat {repeat}')
    checks.update({(f'repeat_{repeat}_' if len(repetitions) > 1 else '') + name: passed
                   for name, passed in validate(group).items()})

result = {
    'checked_at': datetime.now().astimezone().isoformat(),
    'built_executable_sha256': exe_sha,
    'passed': sum(checks.values()), 'total': len(checks), 'checks': checks,
    'inventory': {k: inventory[k] for k in ('image_sha256', 'widget_count', 'controls_count', 'android_handler_count')},
    'unreviewed_classes': [r['class'] for r in inventory['rows'] if r['route']['status'] == 'unreviewed'],
    'android_reference_only_families': [f['class'] for f in families if f['status'] == 'reference-only'],
    'observed_classes': sorted(set(re.findall(r'PvZ input screen (.+?) vtable', '\n'.join(logs.values())))),
    'native_runs': {k: {**v, 'log_sha256': sha(v['log']), 'replay_sha256': sha(root / v['replay'])} for k, v in runs['runs'].items()},
    'source_sha256': source_hashes(root),
    'profile_baseline': runs['profile_baseline'], 'profile_sha256': runs['profile_sha256'],
    'rng_seed': runs['rng_seed'],
    'runs': runs['runs'], 'attempts': runs['attempts'],
    'scope': 'Five native PC-mode replays and actual game-state changes. Inventory classification and handler dispatch are not proof of full screen/gameplay coverage. Fixtures use a separate copied profile.',
    'manual_pending': [
        'Physical controller switching and Console behavior on the newly covered screens',
        'Long Almanac descriptions: scroll amount and boundaries (automated route used short descriptions)',
        'Filled Mail inbox, Credits, all Help/House pages and Leaderboard data',
        'VS Setup with two controllers, Ready, Results and modal cleanup',
        'Locked/unlocked garden switches, all inventories, zero food and tree growth tutorials',
        'All Vasebreaker/I Zombie variants and bonus modes; zombie hand/placement preview appearance',
        'Cob Cannon targeting, Last Stand initial stage, EditWidget keyboard input',
        'Platform invitation/profile/network services and secondary-player mouse input'
    ]
}
(root / 'runtime-data/input-coverage-validation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({k: result[k] for k in ('checked_at', 'built_executable_sha256', 'passed', 'total', 'checks', 'observed_classes')}, indent=2))
if result['passed'] != result['total'] or result['unreviewed_classes']:
    sys.exit(1)
