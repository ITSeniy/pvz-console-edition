"""Verify current menu/garden behavior and immutable replay evidence."""
import json
import re
from datetime import datetime
from pathlib import Path
from input_audit_manifest import sha, source_hashes, verify, ROOT

data = json.loads((ROOT / 'runtime-data/services-feedback-runs.json').read_text(encoding='utf-8-sig'))
logs = {name: Path(run['log']).read_text(encoding='utf-8', errors='replace') for name, run in data['runs'].items()}
checks = {}
def check(name, value):
    checks[name] = bool(value)
for name in ('zen', 'garden_followup', 'garden_polish', 'menus', 'settings', 'tree', 'tree_feeding', 'tree_intro', 'tree_purchase', 'almanac', 'quit'):
    check(name + '_completed', re.search(r'PvZ (?:frame )?replay completed ', logs[name]) and '[critical]' not in logs[name] and '[FATAL]' not in logs[name])
    frames = list(Path(data['runs'][name]['frames']).glob('frame-*.ppm'))
    check(name + '_frames', len(frames) == data['runs'][name]['expected_frames'])
zen = logs['zen']
check('pc_empty_hand_requires_selection', zen.count('PvZ Zen empty hand requires tool selection') >= 2)
check('pc_tools_reset_after_use', all(re.search(rf'PvZ Zen applied tool {tool} hit 1[^\n]*cursor now 0', zen) for tool in (9,10)))
check('pc_toolbar_persistent_highlight_suppressed', 'PvZ PC garden persistent toolbar highlight suppressed' in zen)
check('console_tool_replaces_host_cursor', 'PvZ garden item cursor true mode console' in zen)
check('garden_menu_shop_hover_animated', all(f'PvZ garden button animated hover {button}' in zen for button in (1100,1103)))
check('switch_tool_while_carrying', 'tool 7 cursor 14 -> 10' in zen)
check('pc_and_console_toolbar', zen.count('PvZ garden toolbar tool 7') >= 2 and 'PvZ replay mode_console' in zen)
check('native_plant_tool_action', 'PvZ Zen applied tool 10 hit 1' in zen)
check('locked_garden_icons_not_activated', 'PvZ garden toolbar switched location ' not in zen)
garden = logs['garden_followup']
near = garden.split('PvZ replay fixture garden coin at ', 1)[-1].split('PvZ replay left_down (287, 225)', 1)[0]
check('garden_pc_near_coin_misses', re.search(r'PvZ garden coin hit 120,144[^\n]*state 3 hit false', near))
check('garden_pc_near_coin_tool_works', 'PvZ Zen applied tool 9 hit 1' in near)
collecting = garden.split('PvZ replay left_down (287, 225)', 1)[-1].split('PvZ replay left_down (410, 80)', 1)[0]
check('garden_pc_direct_coin_click', 'PvZ Zen collected coin' in collecting)
check('garden_pc_collecting_coin_ignored', re.search(r'PvZ garden coin hit [^\n]*state 7 hit false', collecting))
check('garden_pc_tool_during_coin_collection', 'PvZ Zen applied tool 9 hit 1' in collecting)
pc_switches = garden.split('PvZ replay left_down (410, 80)', 1)[-1].split('PvZ replay left_down (193, 80)', 1)[0]
console_switches = garden.split('PvZ replay mode_console', 1)[-1].split('PvZ replay left_down (410, 80)', 1)[-1]
for mode, segment in (('pc', pc_switches), ('console', console_switches)):
    check(f'garden_{mode}_switches_keep_empty_hand', all(
        f'PvZ garden tool state background {background} slot -1 cursor 0 mode {mode}' in segment
        for background in (6, 7, 8)) and not re.search(r'PvZ garden tool state[^\n]*cursor 9', segment))
    check(f'garden_{mode}_cursor_above_buttons_and_title', all(
        f'PvZ garden cursor overlay area {area} mode {mode}' in garden for area in (1, 2, 3)))
check('garden_console_keeps_native_coin_attraction', 'PvZ console auto collect coin type 1' in garden)
polish = logs['garden_polish']
hover_only = polish.split('PvZ replay move (193, 80)', 1)[-1].split('PvZ replay left_down (193, 80)', 1)[0]
check('garden_pc_hover_sound_once_per_entry', hover_only.count('PvZ service hover sound ') == 3)
check('garden_pc_hover_reentry_sound_and_outline', hover_only.count('PvZ PC garden toolbar hover painted tool 7') == 2)
check('garden_pc_native_hover_outline', 'PvZ PC garden toolbar hover painted tool 6' in hover_only)
check('garden_pc_hover_does_not_select_tool', 'PvZ garden toolbar tool ' not in hover_only and
      'PvZ garden item cursor true mode pc' not in hover_only)
console_hover = polish.split('PvZ replay move (410, 80)', 2)[-1].split('PvZ replay move (400, 300)', 1)[0]
check('garden_console_navigation_arrow', 'PvZ garden toolbar hover 0 -> 15 mode console' in console_hover and
      'PvZ garden item cursor false mode console' in console_hover and
      'PvZ garden item cursor true mode console' not in console_hover)
console_return = polish.split('PvZ replay move (400, 300)', 4)[-1].split('PvZ replay left_down (410, 80)', 1)[0]
check('garden_console_tool_returns_after_navigation_hover', 'PvZ garden item cursor true mode console' in console_return)
menus = logs['menus']
check('cheat_menu', 'PvZ host menu opened: Cheats' in menus)
check('achievements_menu', 'PvZ host menu opened: Achievements' in menus)
check('three_standard_cheats', all(f'PvZ native cheat {i} enabled true' in menus for i in (0, 1, 2)))
check('quit_cancel', 'PvZ PC quit confirmation result 1001' in menus and data['runs']['menus']['survived'])
settings = logs['settings']
check('pc_settings_back', re.search(r'button 4 found true.*button 0 found true', settings, re.S))
check('back_wraps_to_help', 'PvZ input screen HelpTextScreen' in settings)
check('options_restores_controller_focus', 'PvZ settings returned gamepad focus to Options true' in settings)
check('options_native_focus_after_return', 'PvZ main button 5 focus true hover true remembered 5' in settings)
returned = settings.split('PvZ settings returned gamepad focus to Options true')[-1]
check('adventure_not_highlighted_after_settings', not re.search(r'PvZ main button [01] focus (true|false) hover true', returned) and
      not re.search(r'PvZ main button [01] focus true', returned))
tree = logs['tree']
check('tree_food_consumed', sum(int(before) - int(after) == 1 for before, after in
      re.findall(r'PvZ Tree food click hit 18 balance (\d+) -> (\d+)', tree)) >= 3)
check('tree_console_mode', 'PvZ replay mode_console' in tree)
console_food = logs['tree_feeding']
check('tree_console_food_cursor_cleared_after_feed', 'PvZ Tree feeding cursor cleared mode console' in console_food)
check('tree_console_no_cursor_during_food_animation', 'PvZ Tree food cursor ready false cursor 0 mode console' in console_food and
      'PvZ Tree food cursor ready false cursor 17 mode console' not in console_food)
check('tree_console_food_cursor_returns_when_ready', 'PvZ Tree food cursor ready true cursor 17 mode console' in console_food)
check('tree_console_feed_with_food_remaining', 'PvZ Tree food click hit 18 balance 1005 -> 1004' in console_food and
      'PvZ Tree food click hit 18 balance 1004 -> 1003' in console_food)
returned_garden = console_food.split('PvZ garden toolbar switched location 17', 1)[-1]
check('tree_console_return_to_garden_keeps_arrow', re.search(
      r'PvZ garden tool state background \d+ slot -1 cursor 0 mode console', returned_garden) and
      'PvZ garden item cursor true mode console' not in returned_garden)
check('native_toolbar_garden_switch', 'PvZ garden toolbar switched location 17' in tree)
check('cheats_use_native_board', all(f'PvZ native cheat {i} enabled true' in tree for i in (0, 1, 2)))
intro = logs['tree_intro']
check('tree_first_visit_dave_advanced', any(before != after for before, after in
      re.findall(r'PvZ garden Dave click mode 51 message (-?\d+) -> (-?\d+)', intro)))
check('tree_first_visit_dave_closed', re.search(r'PvZ garden Dave click mode 51 message \d+ -> -1', intro))
check('tree_first_visit_food_consumed', any(int(before) > int(after) for before, after in
      re.findall(r'PvZ Tree food click hit 18 balance (\d+) -> (\d+)', intro)))
purchase = logs['tree_purchase']
check('tree_purchased_from_dave', any(int(before) - int(after) == 1000 for before, after in
      re.findall(r'PvZ Shop purchase item 27 coins (\d+) -> (\d+)', purchase)))
check('tree_purchase_dialog_clickable', 'PvZ garden Dave click mode 51 message 3200 -> -1' in purchase)
check('tree_purchase_native_starting_food', 'PvZ Tree food click hit 18 balance 1005 -> 1004' in purchase)
almanac = logs['almanac']
check('almanac_plant_hover', re.search(r'PvZ Almanac hover plant [0-9]+ zombie -1', almanac))
check('almanac_zombie_hover', re.search(r'PvZ Almanac hover plant -1 zombie [0-9]+', almanac))
for page in (1,2):
    check(f'almanac_page_{page}_native_grid_hover', len(set(re.findall(rf'PvZ Almanac animated grid page {page} hover (\d+) selection', almanac))) >= 2)
    check(f'almanac_page_{page}_click_changes_selection',
          ('PvZ Almanac selection page 1 plant 11 zombie 0' in almanac and
           'PvZ Almanac animated grid page 1 hover 10 selection 11' in almanac) if page == 1 else
          ('PvZ Almanac selection page 2 plant 11 zombie 6' in almanac and
           'PvZ Almanac animated grid page 2 hover 7 selection 6' in almanac))
check('quit_clean_exit', 'PvZ PC quit confirmation result 1000' in logs['quit'] and
      'Window closing, shutting down...' in logs['quit'] and data['runs']['quit']['exit_code'] == 0)
baseline = Path(data['profile_baseline'])
profile_hashes = {p.relative_to(baseline).as_posix(): sha(p) for p in baseline.rglob('*') if p.is_file()}
check('original_profile_unchanged', profile_hashes == {
    p.relative_to(ROOT / 'runtime-data/zen-shop-probe-user').as_posix(): sha(p)
    for p in (ROOT / 'runtime-data/zen-shop-probe-user').rglob('*') if p.is_file()})
data.update(checked_at=datetime.now().astimezone().isoformat(),
            built_executable_sha256=sha(ROOT / 'build/pvz-console-edition.exe'), source_sha256=source_hashes(),
            checker_sha256=sha(Path(__file__)),
            profile_sha256=profile_hashes, checks=checks, passed=sum(checks.values()), total=len(checks),
            frame_sha256={str(p): sha(p) for run in data['runs'].values() for p in Path(run['frames']).glob('frame-*.ppm')},
            limitations='Guest frames exclude host ImGui menus. Physical gamepad, audible sound, and subjective cursor smoothness still require manual play testing.')
data['evidence_errors'] = verify(data)
(ROOT / 'runtime-data/services-feedback-validation.json').write_text(json.dumps(data, indent=2), encoding='utf-8')
print(json.dumps({'passed': data['passed'], 'total': data['total'], 'failed': [k for k, v in checks.items() if not v],
                  'evidence_errors': data['evidence_errors']}, indent=2))
raise SystemExit(bool(data['evidence_errors']) or not all(checks.values()))
