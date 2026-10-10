"""Verify native shovel tutorial progression for the console mouse replay."""
import argparse
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
args = parser.parse_args()
text = args.log.read_text(encoding='utf-8')
steps = re.findall(r'PvZ console shovel press completed (true|false) tutorial (\d+) -> (\d+)', text)
checks = {
    'empty_cell_does_not_complete': bool(steps) and steps[0] == ('false', '15', '15'),
    'first_press_digs': any(s[0] == 'true' and s[1] == '15' for s in steps),
    'three_successful_digs': sum(s[0] == 'true' for s in steps) == 3,
    'last_dig_completes': any(s[0] == 'true' and s[2] == '18' for s in steps),
    'gameplay_ready_after_dave': 'PvZ replay wait_board ready' in text,
    'no_crash_or_timeout': not re.search(r'\[FATAL\]|\[critical\]|timed out|Unhandled guest', text),
}
result = {'log': str(args.log.resolve()), 'replay': 'tests/replays/level_5_console_shovel.csv',
          'steps': steps, 'checks': checks}
Path('runtime-data/console-shovel-latest.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
if not all(checks.values()): raise SystemExit(1)
