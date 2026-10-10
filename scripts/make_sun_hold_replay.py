"""Hold LMB and cross the lawn, then collect naturally spawned suns by clicks."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
rows = (root / 'tests/replays/adventure_mouse.csv').read_text().splitlines()
points = [((x + 240) / 1.6, (y + 60) / 1.2)
          for y in range(100, 551, 50) for x in range(40, 761, 40)]
rows.append('22,700,500,left_down')
for index, (x, y) in enumerate(points):
    rows.append(f'{23 + index * .05:.3f},{x:.3f},{y:.3f},move')
rows.append('34,625,383.333,left_up')
for start in (36, 48):
    for index, (x, y) in enumerate(points):
        time = start + index * .05
        rows.append(f'{time:.3f},{x:.3f},{y:.3f},left_down')
        rows.append(f'{time + .025:.3f},{x:.3f},{y:.3f},left_up')
rows.append('58,400,300,move')
(root / 'tests/replays/sun_hold_regression.csv').write_text('\n'.join(rows) + '\n')
