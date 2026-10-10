"""Check real guest navigation, activation and input switching replay logs."""
import argparse
import json
import re
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('mouse_log', type=Path)
p.add_argument('controller_log', type=Path)
a = p.parse_args()
mouse = a.mouse_log.read_text(encoding='utf-8')
pad = a.controller_log.read_text(encoding='utf-8')
scrolls = [tuple(map(int, m)) for m in re.findall(r'PvZ challenge scroll (\d+) -> (\d+) \((\d+) rows\)', mouse)]
checks = {
    'replays_valid': not re.search(r'Invalid input replay|Cannot open input replay|timed out', mouse + pad),
    'no_guest_crash': not re.search(r'\[FATAL\]|\[critical\]|Unhandled guest', mouse + pad),
    'hover_and_leave': 'PvZ challenge hover -1' in mouse and bool(re.search(r'PvZ challenge hover \d+', mouse)),
    'cancel_off_row': mouse.count('PvZ challenge click cancelled') >= 2,
    'wheel_stays_in_range': bool(scrolls) and all(0 <= x <= max(0, n - 4) and 0 <= y <= max(0, n - 4) for x, y, n in scrolls),
    'wheel_reaches_both_ends': any(y == 0 for x, y, n in scrolls) and any(y == max(0, n - 4) for x, y, n in scrolls),
    'locked_and_available_activation': bool(re.search(r'PvZ challenge activate \d+ locked true', mouse)) and bool(re.search(r'PvZ challenge activate \d+ locked false', mouse)),
    'reopen_after_back': mouse.count('PvZ menu activate button 12') >= 2,
    'controller_up_down_confirm': all(f'PvZ native challenge pad button {i} routed' in pad for i in (0, 1, 6)),
    'automatic_input_switch': 'PvZ input owner: controller' in pad and 'PvZ input owner: mouse' in pad,
}
result = {'mouse_log': str(a.mouse_log.resolve()), 'controller_log': str(a.controller_log.resolve()), 'checks': checks,
          'visual_checks': 'Inspect captures separately for row expansion, contraction, pressed offset and the entered game.'}
Path('runtime-data/minigames-navigation-latest.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(checks, indent=2))
if not all(checks.values()):
    raise SystemExit(1)
