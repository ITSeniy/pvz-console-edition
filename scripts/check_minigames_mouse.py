"""Check native gameplay evidence from the minis_*.csv diagnostic replays.

Usage: python scripts/check_minigames_mouse.py chooser=logs/... zombotany=logs/...
       bowling=... invisi=... slot=... beghouled=... swipe=... twist=...
       quarium=... scroll=...
Checks actions and native outcomes, not merely survival of the process.
"""
from pathlib import Path
import json
import re
import sys

names = ('chooser', 'zombotany', 'bowling', 'invisi', 'slot', 'beghouled',
         'swipe', 'twist', 'quarium', 'scroll')
paths = dict(arg.split('=', 1) for arg in sys.argv[1:])
if set(paths) != set(names):
    sys.exit('Provide exactly these name=log arguments: ' + ', '.join(names))
logs = {name: Path(paths[name]).read_text(encoding='utf-8') for name in names}
checks = {}
for name, log in logs.items():
    checks[name + '_native_run'] = ('Direct3D 12 device and OS features' in log and
        not any(s in log.lower() for s in ('[critical]', '[fatal]', 'timed out',
                                          'invalid input replay', 'access violation')))

chooser = logs['chooser']
checks['chooser_shop_almanac_return'] = all(s in chooser for s in (
    'chooser button 105 activated', 'mouse Shop back', 'chooser button 103 activated',
    'mouse Almanac back', 'chooser click seed 0 state 3', 'chooser click seed 0 state 1'))
checks['chooser_back_button_click'] = ('dialog activate button 1101' in chooser and
                                      'mouse Shop back' in chooser)
zombotany = logs['zombotany']
checks['zombotany_imitater_and_start'] = all(s in zombotany for s in (
    'challenge activate 15 locked false', 'imitater copy seed 0',
    'chooser button 100 activated', 'wait_board ready'))
checks['zombotany_planting'] = ('mouse pick packet 1 allowed true' in zombotany and
                                'mouse planted seed' in zombotany)
checks['bowling_two_conveyor_throws'] = (logs['bowling'].count('mouse used packet 0') >= 2 and
                                        'challenge activate 16 locked false' in logs['bowling'])
checks['invisi_conveyor_plant'] = ('challenge activate 20 locked false' in logs['invisi'] and
                                   'mouse used packet 0' in logs['invisi'])
slot = logs['slot']
checks['slot_lever_starts_roll'] = bool(re.search(r'minigame 19 field click .* hit 22 state 4', slot))
checks['slot_dropped_packet_pick_and_plant'] = all(s in slot for s in (
    'fixture Slot Machine dropped seed', 'mouse dropped seed picked true',
    'mouse dropped seed placed true'))
pickups = re.findall(r'dropped packet pickup caller ([0-9A-F]+) picked 0 seed \d+', slot)
checks['slot_only_explicit_packet_pickups'] = bool(pickups) and all(c == '8216EFAC' for c in pickups)

def purchase(log, mode, price):
    return any(int(before) - int(after) == price and upgrades == '1/0/0'
        for before, after, upgrades in re.findall(
            rf'minigame {mode} buy packet 0 type 7 active \d sun (\d+) -> (\d+) upgrades (\d/\d/\d)', log))

def matches_increase(log, mode):
    scores = [int(n) for n in re.findall(rf'puzzle mode {mode} state \d matches (\d+)', log)]
    return bool(scores) and max(scores) > scores[0]

checks['beghouled_upgrade_purchase'] = purchase(logs['beghouled'], 21, 1000)
checks['beghouled_two_click_match'] = ('PvZ replay left_up (200, 408)' in logs['beghouled'] and
    bool(re.search(r'Beghouled swap .* state 1 capture 0', logs['beghouled'])) and
    matches_increase(logs['beghouled'], 21))
checks['beghouled_swipe_swap'] = ('PvZ replay move (250, 408)' in logs['swipe'] and
    bool(re.search(r'Beghouled swap .* state 1 capture 0', logs['swipe'])))
checks['twist_upgrade_purchase'] = purchase(logs['twist'], 25, 1000)
checks['twist_rotation_match'] = (bool(re.search(r'minigame 25 field click .* state 1', logs['twist'])) and
                                  matches_increase(logs['twist'], 25))
quarium = logs['quarium']
checks['quarium_fish_purchase'] = 'minigame 24 buy packet 0 type 59 active 1 sun 400 -> 300' in quarium
checks['quarium_three_food_clicks'] = (len(re.findall(r'minigame 24 field click .* hit 0 state 0', quarium)) >= 3 and
                                       bool(re.search(r'PvZ controls .* sun 285 cursor', quarium)))
scroll = logs['scroll']
checks['rapid_scroll_reversals_reach_correct_row'] = all(s in scroll for s in (
    'challenge scroll 5 -> 6', 'challenge scroll 13 -> 12', 'challenge scroll 4 -> 3',
    'challenge activate 19 locked false', 'wait_board ready'))

report = {'logs': {k: str(Path(v).resolve()) for k, v in paths.items()},
          'passed': sum(checks.values()), 'total': len(checks), 'checks': checks,
          'limits': ['Partial gameplay scenarios; not full minigame completion.',
                     'Physical controller and 75 Hz scanout require hardware testing.',
                     'Slot replay creates a native dropped packet; RNG odds are unchanged outside replay.']}
out = Path('runtime-data/minigames-mouse-validation.json')
out.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
sys.exit(0 if all(checks.values()) else 1)
