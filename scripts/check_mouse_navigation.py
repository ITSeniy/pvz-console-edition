"""Check native menu press, Help Back/RMB and fullscreen dialog dragging."""
import argparse
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
parser.add_argument('--gameplay', action='store_true')
args = parser.parse_args()
text = args.log.read_text(encoding='utf-8')
checks = {'no_crash_or_timeout': not re.search(r'\[FATAL\]|\[critical\]|Unhandled guest|timed out', text)}
if args.gameplay:
    checks.update({
        'native_press_draw': 'PvZ menu pressed draw button 0' in text,
        'menu_resume_reopen': text.count('PvZ mouse Menu opened') == 2,
        'mouse_pad_mouse': 'PvZ input owner: controller' in text and 'PvZ input owner: mouse' in text,
        'one_successful_plant': text.count('PvZ mouse planted seed') == 1,
        'lift_at_selection': text.count('PvZ mouse sound seed lift') == 2,
        'no_lift_at_planting': text.count('PvZ mouse deferred seed lift suppressed') == 1,
        'cooldown_buzzer': 'PvZ mouse sound buzzer' in text and 'pick packet 0 allowed false' in text,
    })
else:
    checks.update({
        'native_press_draw': 'PvZ menu pressed draw button 5' in text,
        'back_click': 'PvZ dialog activate button 1101' in text,
        'both_help_screens_closed': text.count('PvZ mouse help closed') == 2,
        'right_click': 'PvZ replay right_down (400, 300)' in text,
        'drag_outside_board': bool(re.search(r'PvZ dialog drag 193,32 -> -\d+,\d+', text)),
        'button_outside_board': 'PvZ replay left_down (130, 383)' in text and
            text.rfind('PvZ dialog activate button 4') > text.rfind('PvZ dialog drag'),
        'vsync_off_on_off': re.findall(r'PvZ in-game setting pvz_vsync = (\w+) \(saved true\)', text) ==
            ['false', 'true', 'false'],
        'vsync_present_paths': text.count('DXGI tearing false, presentation thread guest output') >= 2 and
            text.count('DXGI tearing true, presentation thread guest output') >= 2,
    })
print(json.dumps(checks, indent=2))
if not all(checks.values()): raise SystemExit(1)
