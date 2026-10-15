"""Check processed native events and UI recovery in mouse_transport.csv."""
from pathlib import Path
import argparse
import json
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
args = parser.parse_args()
log = args.log.read_text(encoding='utf-8')
events = re.findall(r'PvZ mouse event (\d+) generation (\d+) kind (\d+) point ([\d.-]+),([\d.-]+) click (-?\d+) down (true|false) delta (-?\d+) cancel (\d+)', log)
up_move = any(a[2:7] == ('1', '398', '405', '1', 'false') and
              b[2:5] == ('0', '550', '300') for a, b in zip(events, events[1:]))
checks = {
    'native_run': 'Direct3D 12 device and OS features' in log and not any(
        bad in log.lower() for bad in ('[fatal]', '[critical]', 'access violation', 'timed out', 'invalid input replay')),
    'completed': bool(re.search(r'PvZ replay completed (?:.*[/\\])?mouse_transport\.csv', log)),
    'three_atomic_batches': log.count('PvZ replay batch published') == 3,
    'processed_up_before_later_move': up_move,
    'options_opened_three_times': log.count('PvZ menu activate button 5') == 3,
    'back_activated_only_three_times': log.count('PvZ dialog activate button 4') == 3,
    'focus_and_mode_orphan_releases_ignored': log.count('PvZ mouse unmatched button') >= 2,
    'partial_wheel_reset': re.findall(r'mouse wheel event \d+ delta 60 steps (\d+)', log) == ['0', '1'],
    'mode_round_trip': 'PvZ cursor mode: console' in log and log.count('PvZ cursor mode: pc') >= 2,
    'idle_ready': 'PvZ replay wait_idle ready' in log,
}
print(json.dumps(checks, indent=2))
raise SystemExit(not all(checks.values()))
