"""Check native gameplay replay heavy_weapon_fire_buy.csv."""
from pathlib import Path
import json
import re
import sys

path = Path(sys.argv[1])
log = path.read_text(encoding='utf-8')
buys = [tuple(map(int, row)) for row in re.findall(
    r'Heavy Weapon buy packet (\d+) sun (\d+) -> (\d+) weapon (\d+) missiles (\d+)', log)]
def interval(start, end):
    a = log.find(start)
    b = log.find(end, a + len(start)) if a >= 0 else -1
    return log[a:b] if b > a >= 0 else ''
def quiet(start, end):
    part = interval(start, end)
    return bool(part) and 'Heavy Weapon fire direction' not in part

held = False
invalid_shots = 0
for line in log.splitlines():
    if 'Heavy Weapon LMB firing ' in line:
        held = line.endswith('true')
    elif ('Heavy Weapon LMB released' in line or 'PvZ input owner:' in line or
          'PvZ cursor mode:' in line):
        held = False
    elif 'Heavy Weapon fire direction' in line and not held:
        invalid_shots += 1
idle = interval('PvZ replay move (400, 200)', 'PvZ Heavy Weapon LMB firing true')
idle_angles = [float(a) for a in re.findall(r'Heavy Weapon mouse .* angle ([^\r\n ]+)', idle)]
checks = {
    'valid_native_run': 'challenge activate 18 locked false' in log and
        not any(s in log for s in ('Invalid input replay', '[critical]', '[fatal]', 'timed out')),
    'idle_aim_without_autofire': bool(idle_angles) and max(idle_angles) - min(idle_angles) > .2 and
        'Heavy Weapon fire direction' not in idle,
    'hold_to_fire_and_release': log.count('Heavy Weapon fire direction') > 15 and
        log.count('Heavy Weapon LMB firing true') >= 4 and
        log.count('Heavy Weapon LMB released') >= 6 and invalid_shots == 0,
    'repeater_bought_at_native_price': any(p == 0 and before - after == 200 and weapon == 2
        for p, before, after, weapon, missiles in buys),
    'gatling_bought_at_native_price': any(p == 1 and before - after == 400 and weapon == 3
        for p, before, after, weapon, missiles in buys),
    'auxiliary_missile_bought': any(p == 2 and before - after == 200 and missiles == 1
        for p, before, after, weapon, missiles in buys),
    'disabled_card_does_not_charge_twice': any(p == 0 and before == after and weapon == 2
        for p, before, after, weapon, missiles in buys),
    'insufficient_sun_does_not_buy': any(p == 3 and before == after == 0
        for p, before, after, weapon, missiles in buys),
    'seed_drag_never_starts_fire': all(quiet(a, b) for a, b in [
        ('Heavy Weapon buy packet 0 sun 1000', 'PvZ Heavy Weapon LMB firing true'),
        ('Heavy Weapon buy packet 2', 'PvZ Heavy Weapon LMB firing true')]),
    'bank_hover_suspends_existing_fire': quiet('PvZ replay move (220, 80)', 'PvZ replay move (520, 250)'),
    'console_mouse_buys_and_fires': 'PvZ replay mode_console' in log and
        any(p == 3 and before - after == 100 for p, before, after, weapon, missiles in buys) and
        'Heavy Weapon fire direction' in interval('PvZ replay mode_console', 'PvZ replay mode_pc'),
    'pause_cancels_hold_until_new_press': quiet('PvZ replay move (510, 250)', 'PvZ replay left_down (510, 250)') and
        'native board pad button 5 routed' in log,
}
report = {'log': str(path.resolve()), 'passed': sum(checks.values()), 'total': len(checks),
          'invalid_shots': invalid_shots, 'purchases': buys, 'checks': checks}
Path('runtime-data/heavy-weapon-fire-buy-latest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
sys.exit(0 if all(checks.values()) else 1)
