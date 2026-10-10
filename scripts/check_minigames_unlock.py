"""Check five isolated native mini-game launches after editing the test save."""
import json
from pathlib import Path
import re

checks = {}
logs = {}
for challenge in range(30, 35):
    result = json.loads(Path(f'runtime-data/minigames-unlock-{challenge}/result.json').read_text(encoding='utf-8-sig'))
    log = '\n'.join(Path(name).read_text(encoding='utf-8') for name in result['log_files'])
    marker = f'PvZ challenge activate {challenge} locked false'
    following = log.split(marker, 1)[1] if marker in log else ''
    checks[str(challenge)] = {
        'unlocked_native_activation': marker in log,
        'native_board_rendered': 'PvZ board draw translate' in following,
        'survived': result['survived'],
        'no_fatal_or_invalid_replay': not re.search(r'\[FATAL\]|\[critical\]|Unhandled guest|Invalid input replay|timed out', log),
    }
    logs[str(challenge)] = result['log_files']
report = {'checks': checks, 'logs': logs,
          'scope': 'Native menu activation and entry to each board/seed chooser; full gameplay is not tested.'}
Path('runtime-data/minigames-unlock-validation.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(checks, indent=2))
if not all(all(row.values()) for row in checks.values()):
    raise SystemExit(1)
