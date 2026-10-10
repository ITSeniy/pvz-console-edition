"""Validate native Zen/store/almanac replay logs; no game state is mocked."""
from pathlib import Path
import hashlib, json, re, sys
paths = dict(arg.split('=', 1) for arg in sys.argv[1:])
required = {'zen', 'zen_modes', 'shop', 'almanac', 'regression'}
if set(paths) != required:
    sys.exit('Provide zen, zen_modes, shop, almanac, regression name=log arguments')
logs = {k: Path(p).read_text(encoding='utf-8') for k,p in paths.items()}
checks = {}
for name, log in logs.items():
    checks[name + '_native_run'] = 'Direct3D 12 device and OS features' in log and not any(
        s in log.lower() for s in ('[fatal]', '[critical]', 'exception', 'access violation', 'timed out', 'invalid input replay'))
z, modes, shop, almanac, regression = (logs[k] for k in ('zen', 'zen_modes', 'shop', 'almanac', 'regression'))
checks['glove_pickup'] = bool(re.search(r'Zen applied tool 14 hit 1 .* cursor now 3', z))
checks['glove_placement_empty_cell'] = bool(re.search(r'Zen applied tool 3 hit 0 .* cursor now 14', z))
checks['fertilizer_native_dispatch'] = 'Zen picked tool hit 7 cursor 10' in z and 'Zen applied tool 10 hit 1' in z
checks['right_click_cancel'] = z.count('Zen tool cancelled') >= 2
checks['watering_after_shop_return'] = z.find('Zen Shop returned') >= 0 and 'Zen applied tool 9 hit 1' in z[z.find('Zen Shop returned'):]
checks['console_wheel_native_tool_cycle'] = bool(re.search(r'Zen scroll tool (-?\d+) -> (-?\d+)', modes)) and any(
    a != b for a,b in re.findall(r'Zen scroll tool (-?\d+) -> (-?\d+)', modes))
checks['input_modes_and_controller_return'] = all(s in modes for s in ('cursor mode: console', 'cursor mode: pc', 'input owner: controller', 'input owner: mouse'))
checks['shop_purchase_correct_price'] = 'Shop purchase item 22 coins 99999 -> 99899' in shop
checks['shop_purchase_once'] = shop.count('Shop purchase item') == 1
cancel_start = shop.find('replay left_down (463, 220)')
valid_start = shop.find('replay left_down (463, 220)', cancel_start + 1)
checks['shop_release_on_other_item_cancels'] = cancel_start >= 0 and valid_start > cancel_start and 'dialog button id 1000' not in shop[cancel_start:valid_start]
checks['shop_wheel_both_directions'] = all(s in shop for s in ('Shop scroll page 0 -> 1', 'Shop scroll page 2 -> 1'))
checks['shop_page_arrow_and_back'] = 'Shop page button next' in shop and 'mouse Shop back' in shop
checks['almanac_both_sections'] = all(s in almanac for s in ('Almanac opened page 1', 'Almanac opened page 2'))
checks['almanac_distinct_plants'] = len(set(re.findall(r'Almanac selection page 1 plant (\d+)', almanac))) >= 2
checks['almanac_distinct_zombies'] = len(set(re.findall(r'Almanac selection page 2 plant \d+ zombie (\d+)', almanac))) >= 2
checks['almanac_back_and_right_click'] = almanac.count('mouse Almanac back') >= 3 and 'replay right_down' in almanac
checks['chooser_shop_almanac_round_trip'] = all(s in regression for s in ('chooser button 103 activated', 'chooser button 105 activated', 'mouse Shop back', 'mouse Almanac back', 'chooser click seed'))
result = {'checked_at': __import__('datetime').datetime.now().astimezone().isoformat(),
    'built_executable_sha256': hashlib.sha256(Path('build/pvz-console-edition.exe').read_bytes()).hexdigest(),
    'native_logs': {k:str(Path(p).resolve()) for k,p in paths.items()},
    'passed': sum(checks.values()), 'total':len(checks), 'checks':checks,
    'scope': 'Native dispatch, state transitions, purchase charge, input ownership and chooser regression. Visual watering/plant movement/tool rendering checked separately. Not all garden inventories or tutorials tested.'}
Path('runtime-data/services-mouse-validation.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
if result['passed'] != result['total']: sys.exit(1)
