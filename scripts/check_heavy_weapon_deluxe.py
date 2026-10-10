"""Check Deluxe-style native steering using heavy_weapon_deluxe.csv."""
from pathlib import Path
import json
import math
import re
import sys

path = Path(sys.argv[1])
log = path.read_text(encoding="utf-8")
pattern = (r'Heavy Weapon mouse cursor ([^,]+),([^ ]+) carriage ([^ ]+) -> ([^ ]+) '
           r'y ([^ ]+) aim ([^,]+),([^ ]+) angle ([^\r\n ]+)')

def samples(text):
    return [tuple(map(float, row)) for row in re.findall(pattern, text)]

def between(start, end):
    first = log.find(start)
    last = log.find(end, first + len(start)) if first >= 0 else -1
    return log[first:last] if last > first >= 0 else ""

rows = samples(log)
valid = [r for r in rows if r[0] != -999]
move_errors, aim_errors = [], []
for cx, cy, x, after, y, ax, ay, angle in valid:
    dx, dy = cx - x - 60, y + 20 - cy
    theta = 0 if abs(dx) < .001 else max(-math.pi/2, min(math.pi/2, math.atan2(dx, dy)))
    body = y + 35
    radius = math.hypot(dx, body-cy) if cy < body else abs(dx)
    gain = max(0, min(1, (radius-50)/50))
    step = max(-2.5, min(2.5, math.degrees(theta)/18 * 1.25 * gain))
    move_errors.append(abs(after - max(25, min(700, x + step))))
    aim_errors.append(max(abs(ax-math.sin(theta)), abs(ay-max(0, math.cos(theta)))))

idle_near = between("PvZ replay move (228.125, 537.5)", "PvZ replay move (237.5, 537.5)")
damping = samples(between("PvZ replay move (237.5, 537.5)", "PvZ replay move (237.5, 550)"))
below = samples(between("PvZ replay move (237.5, 550)", "PvZ replay move (212.5, 520.8333"))
near_aim = samples(between("PvZ replay move (212.5, 520.8333", "PvZ replay left_down (212.5, 520.8333"))
firing_near = between("PvZ replay left_down (212.5, 520.8333", "PvZ replay left_up (200, 525)")
console = between("PvZ replay left_down (228.125, 537.5)", "PvZ replay left_up (228.125, 537.5)")
left_out = samples(between("Heavy Weapon mouse cursor -999,-999", "PvZ replay mode_console"))
quiet = log[:log.find("PvZ Heavy Weapon LMB firing true")]
checks = {
    "valid_native_run": 'challenge activate 18 locked false' in log and bool(rows) and
        not any(s in log.lower() for s in ('[fatal]', '[critical]', 'timed out', 'invalid input replay')),
    "original_movement_formula": bool(move_errors) and max(move_errors) < .002,
    "aim_from_pre_move_position": bool(aim_errors) and max(aim_errors) < .0002,
    "bounded_scaled_base_speed": bool(valid) and all(25-.001 <= r[3] <= 700+.001 and
        abs(r[3]-r[2]) <= 2.501 for r in valid),
    "idle_without_autofire": bool(quiet) and 'Heavy Weapon fire direction' not in quiet,
    "idle_near_zone_stands_and_aims": len(samples(idle_near)) >= 3 and
        all(abs(r[3]-25) < .01 and abs(r[3]-r[2]) < .001 and r[5] > .99 for r in samples(idle_near)),
    "damped_approach_stops_short_of_cursor": len(damping) >= 3 and
        all(25 <= r[3] <= 30.01 for r in damping) and
        any(.001 < r[3]-r[2] < .625 for r in damping) and abs(damping[-1][3]-30) < .03,
    "below_body_uses_horizontal_radius": len(below) >= 3 and
        all(abs(r[3]-30) < .03 and abs(r[3]-r[2]) < .001 for r in below),
    "near_zone_multiple_aim_angles": len(near_aim) >= 3 and
        all(abs(r[3]-r[2]) < .001 for r in near_aim) and
        any(r[5] > .5 for r in near_aim) and any(r[5] < -.9 for r in near_aim) and
        any(abs(r[5]) < .01 and r[6] > .99 for r in near_aim),
    "held_fire_while_stationary": len(samples(firing_near)) >= 3 and
        all(abs(r[3]-r[2]) < .001 for r in samples(firing_near)) and
        firing_near.count('Heavy Weapon fire direction') >= 4,
    "pointer_leave_stops_motion": bool(left_out) and
        all(r[0] == -999 and r[2] == r[3] and r[5] == r[6] == 0 for r in left_out),
    "console_mouse_same_near_zone": len(samples(console)) >= 3 and
        all(abs(r[3]-25) < .01 and abs(r[3]-r[2]) < .001 for r in samples(console)) and
        console.count('Heavy Weapon fire direction') >= 4,
}
report = {"log": str(path.resolve()), "passed": sum(checks.values()), "total": len(checks),
          "samples": len(valid), "maximum_movement_error": max(move_errors, default=None),
          "maximum_aim_error": max(aim_errors, default=None), "checks": checks}
Path("runtime-data/heavy-weapon-deluxe-validation.json").write_text(
    json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
sys.exit(0 if all(checks.values()) else 1)
