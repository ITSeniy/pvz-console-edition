"""Audit native UTF-16 LawnStrings without modifying game packages."""
import argparse, json, re
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('strings',type=Path)
p.add_argument('--output',type=Path,default=Path('runtime-data/text-audit.json'))
a=p.parse_args()
raw=a.strings.read_bytes().decode('utf-16-be').replace('\r','')
# Keys may contain spaces and mixed case (including XBox Coop Survival).
entries={m[1]:m[2].strip() for m in re.finditer(r'^\[([^\]\n]+)\]\n(.*?)(?=^\[[^\]\n]+\]\n|\Z)',raw,re.M|re.S)}
code=Path('src/pc_text.h').read_text()
platform=set(re.findall(r'Entry\{u"([^"]+)"',code))
input_keys=[k for k,v in entries.items() if re.search(r'<[ABXYSLRlr]>|\bLB RB LT RT\b|press button',v,re.I)]
platform_keys=[k for k,v in entries.items() if re.search(r'xbox|console|gamertag|gamer card|storage device|marketplace|trial version',v,re.I)]
result={'source':str(a.strings),'entries_scanned':len(entries),'platform_keys':platform_keys,
 'platform_overrides':sorted(platform),'input_keys':input_keys,
 'controller_only_keys':['2P_JOIN_TEXT','2P_JOINED','P2_JOIN','RECONNECT_SECOND_CONTROLLER_FMT'],
 'uncovered_platform_keys':sorted(set(platform_keys)-platform),
 'notes':['Localization IDs remain unchanged; returned text copies are adapted.',
 'PC: LMB selects seeds and uses the shovel. Console mouse: wheel selects, hold/release RMB digs.',
 'Other actions use current keyboard bindings; second-player join retains controller prompts.']}
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
