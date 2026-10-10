"""Check music channel recovery and PCM output after SFX changes."""
import argparse,json,re
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('log',type=Path);p.add_argument('--require-recovery',action='store_true')
a=p.parse_args();s=a.log.read_text(encoding='utf8')
sliders=[(int(i),float(v)) for i,v in re.findall(r'PvZ audio slider (\d+) value ([\d.]+)',s)]
playing=re.findall(r'PvZ audio menu music playing (true|false) volume ([\d.]+)',s)
last=[float(v) for v in re.findall(r'PvZ audio output RMS ([\deE.+-]+)',s)][-5:]
checks={'sfx_zero_and_restored':any(i==1 and v==0 for i,v in sliders) and any(i==1 and 0<v<1 for i,v in sliders),
 'music_channel_remains_playing':bool(playing) and all(p[0]=='true' for p in playing),
 'audio_output_after_changes':len(last)==5 and min(last)>0.001,
 'no_crash_or_timeout':not re.search(r'\[FATAL\]|\[critical\]|timed out|Unhandled guest',s)}
if a.require_recovery:
 checks['injected_loss_recovered']=s.count('PvZ audio diagnostic dropped menu channel')==1 and s.count('PvZ audio restored lost menu music channel')==1
else:checks['active_music_not_restarted']='PvZ audio restored lost menu music channel' not in s
r={'log':str(a.log.resolve()),'sliders':sliders,'last_rms':last,'checks':checks}
print(json.dumps(r,indent=2));Path('runtime-data/audio-latest.json').write_text(json.dumps(r,indent=2))
if not all(checks.values()):raise SystemExit(1)
