"""Check a native Heavy Weapon run of heavy_weapon_controller.csv."""
from pathlib import Path
import json
import re
import sys

path = Path(sys.argv[1])
log = path.read_text(encoding='utf-8')
samples = [tuple(map(float, match)) for match in re.findall(
    r'Heavy Weapon mouse cursor ([^,]+),([^ ]+) carriage ([^ ]+) -> ([^ ]+) '
    r'y ([^ ]+) aim ([^,]+),([^ ]+) angle ([^\r\n ]+)', log)]
controller_start = log.find('PvZ replay controller_left')
controller_end = log.find('PvZ replay move (500, 180)', controller_start)
controller = log[controller_start:controller_end] if controller_start >= 0 and controller_end > controller_start else ''
native = [tuple(map(float, match)) for match in re.findall(
    r'Heavy Weapon native carriage ([^ ]+) y ([^ ]+) angle ([^\r\n ]+)', controller)]
cancel_start = log.find('Heavy Weapon mouse cursor -999,-999')
cancel_end = log.find('PvZ replay move (400, 200)', cancel_start)
cancel = log[cancel_start:cancel_end] if cancel_start >= 0 and cancel_end > cancel_start else ''
pause_start = log.find('PvZ replay controller buttons 16 ')
pause_end = log.find('PvZ replay controller buttons 16 ', pause_start + 1)
pause = log[pause_start:pause_end] if pause_start >= 0 and pause_end > pause_start else ''
checks = {
    'valid_run': not any(s in log for s in ('Invalid input replay', '[critical]', '[fatal]', 'timed out')),
    'native_heavy_weapon_started': 'challenge activate 18 locked false' in log and bool(samples),
    'bounded_carriage_and_both_edges': bool(samples) and
        all(25 - .01 <= a[3] <= 700 + .01 for a in samples) and
        any(abs(a[3] - 25) < .01 for a in samples) and any(abs(a[3] - 700) < .01 for a in samples),
    'movement_and_stop_without_teleport': bool(samples) and
        all(abs(a[3] - a[2]) <= 2.51 for a in samples) and
        any(a[3] > a[2] for a in samples) and any(a[3] < a[2] for a in samples) and
        any(a[3] == a[2] and a[0] != -999 for a in samples),
    'aim_both_sides_and_up': any(a[5] < -.1 for a in samples) and
        any(a[5] > .1 for a in samples) and any(abs(a[5]) < .01 and a[6] > .99 for a in samples),
    'native_fire': log.count('Heavy Weapon fire direction') >= 10,
    'pointer_leave_stops_fire_and_movement': bool(cancel) and 'Heavy Weapon fire direction' not in cancel and
        any(a[0] == -999 and a[2] == a[3] and a[5] == a[6] == 0 for a in samples),
    'controller_moves_and_aims_independently': bool(native) and
        max(a[0] for a in native) - min(a[0] for a in native) > 20 and
        max(a[2] for a in native) - min(a[2] for a in native) > .2 and
        'Heavy Weapon mouse cursor' not in controller and 'input owner: controller' in controller,
    'mouse_returns_and_both_modes': controller_end >= 0 and 'input owner: mouse' in log[controller_end:] and
        'PvZ replay mode_console' in log and 'PvZ replay mode_pc' in log,
    'pause_and_resume': bool(pause) and 'Heavy Weapon mouse cursor' not in pause and
        'Heavy Weapon mouse cursor' in log[pause_end:] and 'native board pad button 5 routed' in log,
}
report = {'log': str(path.resolve()), 'passed': sum(checks.values()), 'total': len(checks), 'checks': checks}
Path('runtime-data/heavy-weapon-latest.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
sys.exit(0 if all(checks.values()) else 1)
