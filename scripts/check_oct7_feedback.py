"""Validate the October 7 input regression using actual native/backend events."""
import argparse,json,re
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('navigation_log',type=Path);a=p.parse_args();s=a.log.read_text(encoding='utf-8');n=a.navigation_log.read_text(encoding='utf-8')
button_ids=set(map(int,re.findall(r'native board pad button (\d+) routed',s)))
fps=list(map(float,re.findall(r'= ([\d.]+) FPS \(limit 60\)',s)))
def section(start,end):
 i=s.find(start);j=s.find(end,i+len(start));return s[i:j] if i>=0 and j>=0 else ''
hold=section('PvZ replay left_down (640, 320)','PvZ replay left_up (650, 400)')
suns=set(map(int,re.findall(r'board state -?\d+ sun (\d+)',hold)))
drag_release=n.find('PvZ replay left_up (170, 130)');drag_restart=n.find('PvZ replay left_down (170, 130)',drag_release)
checks={
 'no_crash_or_timeout':not re.search(r'\[FATAL\]|\[critical\]|Unhandled guest|timed out',s+n),
 'all_native_controller_buttons':{5,10,11,12,13}<=button_ids,
 'mouse_and_controller_switch':'PvZ input owner: controller' in s and 'PvZ input owner: mouse' in s,
 'paced_60_fps':len(fps)>=5 and all(58<=v<=60.5 for v in fps[2:]),
 'plant_preview':'PvZ mouse planting preview cell' in s,
 'one_plant':s.count('PvZ mouse planted seed')==1,
 'held_mouse_no_sun_gain':bool(hold) and len(suns)==1,
 'release_clears_framework_buttons':'PvZ framework release held 0' in s and not re.search(r'framework release held [1-9]',s),
 'menu_after_hold':s.find('PvZ mouse Menu opened')>s.find('PvZ replay left_up (650, 400)'),
 'ingame_leaderboard_closed':n.count('PvZ mouse house leaderboard closed')==2,
 'nested_dialog_drag':n.count('PvZ dialog drag')==2,
 'drag_stops_after_release':drag_release>=0 and drag_restart>drag_release and 'PvZ dialog drag' not in n[drag_release:drag_restart],
 'intro_setting_saved_twice':re.findall(r'in-game setting pvz_skip_intro = (\w+) \(saved true\)',n)==['false','true'],
}
print(json.dumps({'checks':checks,'native_buttons':sorted(button_ids),'sun_during_hold':sorted(suns),'fps':fps},indent=2))
if not all(checks.values()):raise SystemExit(1)
