"""Check native Shop/Select House behavior in repeatable replay runs."""
from pathlib import Path
from datetime import datetime
import json
import re
from input_audit_manifest import sha, source_hashes, verify

root = Path(__file__).resolve().parents[1]
data = json.loads((root / 'runtime-data/shop-regression-runs.json').read_text(encoding='utf-8-sig'))
checks = {}
frames = {}
for name, run in sorted(data['runs'].items()):
    log = Path(run['log']).read_text(encoding='utf-8')
    def check(label, result):
        checks[name + ':' + label] = bool(result)
    def between(start, end):
        return log.split(start, 1)[-1].split(end, 1)[0] if start in log and end in log else ''
    check('native_run', 'Direct3D 12 device and OS features' in log and not any(
        bad in log.lower() for bad in ('[fatal]', '[critical]', 'access violation', 'timed out', 'invalid input replay')))
    check('replay_complete', 'PvZ replay completed ' + run['replay'] in log)
    sound = between('PvZ replay move (463, 220)', 'PvZ replay move (50, 50)')
    check('one_hover_sound_for_same_item', sound.count('PvZ service hover sound ') == 1 and
          re.search(r'PvZ service hover sound \d+ instance (?!00000000)[0-9A-F]+', sound))
    check('first_gamepad_direction_changes_slot', 'PvZ Shop native navigation 3 slot 1 -> 2 page 0 -> 0' in log)
    check('gamepad_from_mouse_button_changes_page', 'page 0 -> 1' in between(
        'PvZ replay controller buttons 512 ', 'PvZ replay controller buttons 0 '))
    # Neutral rows precede later presses; match native effects independently.
    check('gamepad_from_blank_changes_page', re.search(r'PvZ Shop native navigation 10 slot \d+ -> \d+ page 1 -> 0', log))
    check('open_hatchback_both_devices', 'PvZ Shop open hatchback rendered mouse true' in log and
          'PvZ Shop open hatchback rendered mouse false' in log and
          log.count('PvZ Shop open hatchback rendered mouse true') >= 3)
    check('mouse_controls_remain_visible_under_popup',
          'PvZ Shop mouse controls visible true enabled false' in log and
          'PvZ Shop mouse controls visible true enabled true' in log)
    check('locked_house_and_drag_out_do_not_confirm', 'PvZ Select House native action 6' not in between(
        'PvZ replay left_down (392, 260)', 'PvZ replay left_down (280, 260)') and
          'PvZ Select House hover 2' not in log)
    check('house_hover', 'PvZ Select House hover 0' in log and 'PvZ Select House hover 1' in log)
    check('mouse_house_committed', 'PvZ Select House confirmed 0 profile 0' in log and
          'PvZ Select House native action 6 selected 0 saved 1 -> 0' in log)
    # The last Down on this house is interrupted by a pad press.
    interrupted = log.rsplit('PvZ replay left_down (336, 260)', 1)[-1].split('PvZ replay move (336, 261)', 1)[0]
    check('owner_switch_cancels_mouse_press', 'PvZ input owner: controller' in interrupted and
          'PvZ Select House native action 6' not in interrupted and
          'PvZ Select House confirmed ' not in interrupted and
          log.count('PvZ Select House confirmed ') == 1)
    check('mouse_cancel_keeps_saved_house', 'PvZ Select House native action 7 selected 1 saved 0 -> 0' in log)
    check('gamepad_house_committed', 'PvZ Select House native action 6 selected 1 saved 0 -> 1' in log)
    purchases = [(int(a), int(b)) for a, b in re.findall(r'PvZ Shop purchase item 36 coins (\d+) -> (\d+)', log)]
    check('switch_cost_and_refund', len(purchases) == 3 and [a-b for a,b in purchases] == [30, 0, 30])
    check('return_to_menu', 'PvZ mouse Shop back' in log and 'PvZ input screen MainMenu' in log.rsplit('PvZ Shop purchase item 36', 1)[-1])
    captured = sorted(Path(run['frames']).glob('frame-*.ppm'))
    check('captured_frames', len(captured) == 16)
    frames[name] = {str(p): sha(p) for p in captured}

baseline = Path(data['profile_baseline'])
profile_hashes = {p.relative_to(baseline).as_posix(): sha(p) for p in baseline.rglob('*') if p.is_file()}
checks['original_profile_unchanged'] = profile_hashes == {
    p.relative_to(root / 'runtime-data/zen-shop-probe-user').as_posix(): sha(p)
    for p in (root / 'runtime-data/zen-shop-probe-user').rglob('*') if p.is_file()}
checks['two_repeats_per_mode'] = all(sum(run['mode'] == mode for run in data['runs'].values()) >= 2 for mode in ('pc','console'))
data.update(checked_at=datetime.now().astimezone().isoformat(),
            built_executable_sha256=sha(root / 'build/pvz-console-edition.exe'), source_sha256=source_hashes(),
            profile_sha256=profile_hashes, frame_sha256=frames, checks=checks,
            passed=sum(checks.values()), total=len(checks),
            scope='Native Shop and Select House replays; two runs per cursor mode, controller injection, copied save. Audio checks verify native playback invocation; listening on physical audio output is manual.')
data['evidence_errors'] = verify(data)
(root / 'runtime-data/shop-regression-validation.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
print(json.dumps({'passed': data['passed'], 'total': data['total'],
                  'failed': [k for k,v in checks.items() if not v], 'evidence_errors': data['evidence_errors']}, indent=2))
raise SystemExit(bool(data['evidence_errors']) or not all(checks.values()))
