"""Check the PC settings replay, persistence and unchanged unrelated options."""
import argparse
import json
import re
import tomllib
from datetime import date
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
parser.add_argument('profile', type=Path)
args = parser.parse_args()
text = args.log.read_text(encoding='utf-8')
config_text = (args.profile / 'user' / 'pvz.toml').read_text(encoding='utf-8-sig')
config = tomllib.loads(config_text)
defaults = tomllib.loads(Path('config/pc_defaults.toml').read_text(encoding='utf-8-sig'))
expected = {'pvz_cursor_mode':'pc', 'pvz_vsync':False,
            'pvz_pause_on_focus_loss':False, 'pvz_display_mode':'windowed'}
transitions = re.findall(r'PvZ in-game setting (\w+) = (\w+) \(saved true\)', text)
checks = {
    'native_full_license':'PvZ native full-game license check: true' in text,
    'seven_saved_changes':transitions == [
        ('pvz_cursor_mode','console'),('pvz_cursor_mode','pc'),('pvz_vsync','false'),
        ('pvz_pause_on_focus_loss','false'),('pvz_display_mode','fullscreen'),
        ('pvz_display_mode','borderless'),('pvz_display_mode','windowed')],
    'saved_values':all(config.get(k)==v for k,v in expected.items()),
    'no_duplicate_preferences':all(len(re.findall(r'^'+k+r'\s*=', config_text,re.M))==1 for k in expected),
    'unrelated_preferences_preserved':all(config.get(k)==v for k,v in defaults.items() if k not in expected),
    'native_settings_storage_hidden':'PvZ Change Storage hidden in PC settings' in text,
    'page_back_and_reopen':text.count('PvZ dialog activate button 1002')==4,
    'no_crash_or_timeout':not re.search(r'\[FATAL\]|\[critical\]|Unhandled guest|timed out|could not save',text),
}
result = {'date':str(date.today()), 'log':str(args.log.resolve()), 'profile':str(args.profile.resolve()),
          'checks':checks, 'replay':'tests/replays/game_settings.csv',
          'limitations':['Physical gamepad and monitor refresh synchronisation require hardware testing.']}
Path('runtime-data/game-settings-latest.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(checks,indent=2))
if not all(checks.values()): raise SystemExit(1)
