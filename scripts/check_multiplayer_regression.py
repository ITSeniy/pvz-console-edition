"""Validate local player assignment and native transitions into multiplayer boards."""
import json
import re
from datetime import datetime
from pathlib import Path
from input_audit_manifest import ROOT, sha, source_hashes, verify

data = json.loads((ROOT / 'runtime-data/multiplayer-regression-runs.json').read_text(encoding='utf-8-sig'))
MOUSE, PAD = '18446744073709551615', '88331812753485'
# Feedback round 2026-10-15: player 1 device, whether the run reaches a board.
FOLLOWUP = {'vs_mouse_p2_sun': (PAD, True), 'vs_custom_mouse_plants': (MOUSE, False),
            'vs_custom_mouse_zombies': (MOUSE, False), 'vs_join_mouse': (MOUSE, False),
            'vs_join_pad': (PAD, False), 'vs_pick_sides': (MOUSE, True)}
PLANT_SEED = r'PvZ chooser click seed (?:[0-9]|[1-5][0-9]|60) '
ZOMBIE_SEED = r'PvZ chooser click seed (?:6[1-9]|[7-9][0-9]) '
checks = {}
for name, (first, board) in FOLLOWUP.items():
    run = data['runs'].get(name, {})
    log = Path(run['log']).read_text(encoding='utf-8', errors='replace') if run else ''
    checks[name + '_completed'] = 'PvZ replay completed ' in log and not re.search(
        r'Unhandled guest|access violation|\[FATAL\]|\[critical\]|timed out', log, re.I)
    checks[name + '_frames'] = bool(run) and len(list(Path(run['frames']).glob('frame-*.ppm'))) >= 5
    checks[name + '_first_device_is_player_one'] = f'PvZ local players started: player 1 device {first}' in log
    if board: checks[name + '_native_board'] = bool(re.search(r'PvZ local board mode 76 scene 3 users', log))
def text(name):
    run = data['runs'].get(name)
    return Path(run['log']).read_text(encoding='utf-8', errors='replace') if run else ''
# Sun is clicked, not pulled in by the mouse player's cursor, also as player 2.
sun = text('vs_mouse_p2_sun')
checks['p2_mouse_sun_proximity_blocked'] = 'PvZ mouse proximity collect blocked user 1 type 4' in sun
checks['p2_mouse_sun_hover_blocked'] = 'PvZ mouse hover pickup blocked type 4' in sun
checks['p2_mouse_sun_clicked'] = 'PvZ mouse collect coin type 4' in sun
plants, zombies = text('vs_custom_mouse_plants'), text('vs_custom_mouse_zombies')
checks['custom_mouse_plants_own_panel_only'] = bool(re.search(PLANT_SEED, plants)) and not re.search(ZOMBIE_SEED, plants)
checks['custom_mouse_zombies_own_panel_only'] = bool(re.search(ZOMBIE_SEED, zombies)) and not re.search(PLANT_SEED, zombies)
checks['custom_pad_routed_to_own_panel'] = all('native co-op chooser button 6 user 1 panel 1' in log for log in (plants, zombies))
# Player 2's pad adds a card to its own panel (that bank grows to 2).
checks['custom_pad_picked'] = all(re.search(r'button 6 user 1 panel 1 cursors 0,\d+ banks 2,0', log) for log in (plants, zombies))
checks['join_mouse_rmb_cancels'] = 'PvZ second player prompt mouse B user 0' in text('vs_join_mouse')
checks['join_pad_mouse_lmb_joins'] = all(t in text('vs_join_pad') for t in
    ('PvZ local mouse joined player 2', 'PvZ second player prompt mouse A user 1'))
sides = text('vs_pick_sides')
checks['pick_sides_mouse_icon'] = 'PvZ VS mouse player icons loaded true true' in sides
checks['pick_sides_click_moves'] = all(t in sides for t in (
    'pick side click user 0 target 1 side -1 -> 1', 'pick side click user 0 target -1 side 1 -> -1',
    'pick side click user 0 target 0 side -1 -> 0'))
checks['pick_sides_second_click_ready'] = 'pick side click user 0 target 0 side 0 -> 0 ready true' in sides
checks['pick_sides_right_click_unready'] = 'pick side cancel user 0 side 0 -> 0 ready false' in sides
# Mode selection belongs to player 1 (the mouse here): player 2's pad is ignored.
checks['pick_sides_pad_ignored_in_mode_menu'] = 'PvZ pad button 3 user 1 ignored: menu belongs to player 1' in sides
for name in ('vs_mouse_pad', 'vs_pad_mouse', 'vs_two_pads', 'coop_mouse_pad', 'coop_pad_mouse', 'coop_two_pads'):
    run = data['runs'].get(name, {})
    log = Path(run['log']).read_text(encoding='utf-8', errors='replace') if run else ''
    checks[name + '_completed'] = 'PvZ replay completed ' in log and not re.search(
        r'Unhandled guest|access violation|\[FATAL\]|\[critical\]|timed out', log, re.I)
    checks[name + '_frames'] = bool(run) and len(list(Path(run['frames']).glob('frame-*.ppm'))) >= 10
    first = '18446744073709551615' if name.endswith('mouse_pad') else '88331812753485'
    checks[name + '_first_device_is_player_one'] = f'PvZ local players started: player 1 device {first}' in log
    if name == 'vs_pad_mouse':
        checks['mouse_joins_player_two'] = 'PvZ local mouse joined player 2' in log
        # A second click on the side the badge holds confirms it.
        checks['mouse_player_two_ready'] = 'pick side click user 1 target 1 side 1 -> 1 ready true' in log
    mode = '76' if name.startswith('vs') else '(?:7[7-9]|8[0-9])'
    checks[name + '_native_board'] = bool(re.search(rf'PvZ local board mode {mode} scene 3 users 0,1', log))
    if name.startswith('coop'):
        checks[name + '_both_banks_selected'] = 'PvZ local chooser selected 8 banks 4,4 ready 1' in log
    if name == 'vs_mouse_pad': checks['mouse_player_one_plants'] = 'PvZ mouse planted seed' in log
    # Menus belong to player 1's device: a player 2 mouse cannot press them.
    if name == 'coop_pad_mouse': checks['pad_player_one_owns_menus'] = 'mouse click ignored: menu belongs to player 1' in log
    if name == 'vs_pad_mouse': checks['mouse_player_two_places'] = 'PvZ mouse planted seed' in log

baseline = Path(data['profile_baseline'])
data.update(checked_at=datetime.now().astimezone().isoformat(), source_sha256=source_hashes(),
    profile_sha256={p.relative_to(baseline).as_posix(): sha(p) for p in baseline.rglob('*') if p.is_file()},
    frame_sha256={str(p): sha(p) for run in data['runs'].values() for p in Path(run['frames']).glob('frame-*.ppm')},
    checks=checks, passed=sum(checks.values()), total=len(checks),
    limitations='Two diagnostic physical pad sources and mouse input. Real controllers, rumble and full matches require manual testing.')
data['evidence_errors'] = verify(data)
(ROOT / 'runtime-data/multiplayer-regression-validation.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
print(json.dumps({'passed': data['passed'], 'total': data['total'], 'failed': [k for k,v in checks.items() if not v], 'evidence_errors': data['evidence_errors']}, indent=2))
raise SystemExit(bool(data['evidence_errors']) or not all(checks.values()))
