"""Validate T15 repeat traces in logic steps, without changing input timelines.

Input: t15-gamepad-run.ps1 output root. Output: repeat-proof.json containing
SHA-256s, every hold/delivery, expected intervals and strict off/native equality.
Wall-clock ms and allocator pointers never stand in for the logical counter.
"""
import hashlib
import json
from pathlib import Path
import re
import sys
import argparse

parser=argparse.ArgumentParser()
parser.add_argument('root',type=Path)
parser.add_argument('--coop-repeat',type=Path)
parser.add_argument('--case-repeat',type=Path,action='append',default=[],help='Replace one exact completed OFF/native case pair; retain rejected evidence and validate immutable inputs')
parser.add_argument('--output',type=Path)
args=parser.parse_args()
root=args.root
manifest=root/'summary.json'
summary=json.loads(manifest.read_text(encoding='utf-8-sig'))
sha={str(manifest):hashlib.sha256(manifest.read_bytes()).hexdigest()}
replaced=[]
repeat_roots=([args.coop_repeat] if args.coop_repeat else [])+args.case_repeat
replaced_cases=set()
for repeat_root in repeat_roots:
    attempt_path=repeat_root/'summary.json'
    attempt=json.loads(attempt_path.read_text(encoding='utf-8-sig'))
    sha[str(attempt_path)]=hashlib.sha256(attempt_path.read_bytes()).hexdigest()
    selected=attempt['runs']
    cases={r['case'] for r in selected}
    if len(selected)!=2 or len(cases)!=1 or {r['mode'] for r in selected}!={'off','native'} or any(not r['complete'] or not r['field_coverage'] for r in selected):
        raise ValueError('Case repeat must contain two completed OFF/native runs with required field coverage')
    case=next(iter(cases))
    if (repeat_root==args.coop_repeat and case!='t07b_coop_hold_matrix') or case in replaced_cases:
        raise ValueError('Wrong or duplicate repeated case')
    replaced_cases.add(case)
    replaced_pair=[r for r in summary['runs'] if r['case']==case]
    replaced.extend(replaced_pair)
    for new in selected:
        old=next(r for r in replaced_pair if r['mode']==new['mode'])
        a_path=Path(old['receipt']).parent/'inputs.json';b_path=Path(new['receipt']).parent/'inputs.json'
        a=json.loads(a_path.read_text(encoding='utf-8-sig'));b=json.loads(b_path.read_text(encoding='utf-8-sig'))
        sha[str(a_path)]=hashlib.sha256(a_path.read_bytes()).hexdigest();sha[str(b_path)]=hashlib.sha256(b_path.read_bytes()).hexdigest()
        if any(a[k]!=b[k] for k in ('exe','replay','arguments','baseline','runner_sha256')):raise ValueError('Case repeat changed game inputs')
        receipt=Path(old['receipt']);data=json.loads(receipt.read_text(encoding='utf-8-sig'))
        sha[str(receipt)]=hashlib.sha256(receipt.read_bytes()).hexdigest()
        for p in map(Path,data['log_files']):sha[str(p)]=hashlib.sha256(p.read_bytes()).hexdigest()
    summary['runs']=[r for r in summary['runs'] if r['case']!=case]+selected
runs={}
errors=[]
for run in summary['runs']:
    receipt=Path(run['receipt'])
    sha[str(receipt)]=hashlib.sha256(receipt.read_bytes()).hexdigest()
    smoke=json.loads(receipt.read_text(encoding='utf-8-sig'))
    lines=[]
    for path in map(Path,smoke['log_files']):
        data=path.read_bytes();sha[str(path)]=hashlib.sha256(data).hexdigest()
        lines.extend(data.decode('utf-8-sig',errors='replace').splitlines())
    # Sort complete timestamped records across rotations; preserve within-ms order.
    lines=sorted(lines,key=lambda s:s[:25])
    fields=[]
    for line in lines:
        m=re.search(r'PvZ repeat field ms \d+ step (\d+) user (\d+) mode (\d+) scene (\d+) paused (\d+)',line)
        if m:fields.append(dict(zip(('step','user','mode','scene','paused'),map(int,m.groups()))))
    field_ok=True
    if run['case']=='t07b_coop_hold_matrix':field_ok={f['user'] for f in fields if f['mode']==79 and f['scene']==3}>={0,1}
    if run['case']=='t07b_pause_hold_matrix':field_ok=any(f['mode']==20 and f['scene']==3 for f in fields)
    if not field_ok:errors.append(f"{run['case']}/{run['mode']}: missing target field coverage")
    holds=[];active={}
    for line in lines:
        m=re.search(r'PvZ repeat edge ms \d+ step (\d+) user (\d+) button (\d+) down ([01])',line)
        if m:
            step,user,button,down=map(int,m.groups());key=(user,button)
            if down:
                h={'user':user,'source':button,'down':step,'up':None,'repeats':[],'mirrors':[],'intervals':[]}
                holds.append(h);active[key]=h
            elif key in active:
                active.pop(key)['up']=step
            continue
        m=re.search(r'PvZ repeat event ms \d+ step (\d+) user (\d+) button (\d+) flags (\d+) source (\d+) repeat_count (\d+)',line)
        if m:
            step,user,button,flags,source,count=map(int,m.groups())
            if not count:continue
            key=(user,source)
            if key not in active:
                errors.append(f"{run['mode']}/{run['case']}: repeat without down {m[0]}");continue
            h=active[key]
            if flags==3:
                # 82580ECC mirrors D-pad source16..19 into button0..3 on
                # the SAME repeat step. This is a separate callback, not a
                # second advance of the repeat counter.
                h['mirrors'].append({'step':step,'count':count,'button':button})
                if source<16 or button!=source-16 or not h['repeats'] or h['repeats'][-1]['step']!=step or h['repeats'][-1]['count']!=count:
                    errors.append(f"invalid mirrored repeat {m[0]}")
                continue
            if flags!=1:
                errors.append('unexpected repeat flags '+m[0]);continue
            previous=h['repeats'][-1]['step'] if h['repeats'] else h['down']
            delta=step-previous
            h['repeats'].append({'step':step,'count':count,'flags':flags,'button':button})
            h['intervals'].append(delta)
            # Xbox directions use 25..20 threshold with >, hence first 26.
            # Other keys use threshold100 (first101, then100/99/...20).
            ordinal=len(h['repeats'])
            expected=(26 if source>=16 or source<4 else 101)-min(ordinal-1,6)
            if count!=min(ordinal,6):errors.append('invalid saturated repeat count '+m[0])
            if delta!=expected:
                errors.append(f"{run['mode']}/{run['case']}: user={user} source={source} count={count} delta={delta} expected={expected}")
    counter_errors=[]
    for line in lines:
        m=re.search(r'PvZ native (gamepad_\w+) summary reason replay_end comparisons (\d+) errors (\d+)',line)
        if m and int(m[3]):counter_errors.append(m[0])
    for h in holds:
        if h['source']>=16 and len(h['mirrors'])!=len(h['repeats']):errors.append('missing mirrored callback')
    runs[f"{run['case']}/{run['mode']}"]={'complete':run['complete'],'field_coverage':field_ok,'fields':fields,'holds':holds,'counter_errors':counter_errors}
    errors.extend(counter_errors)
policy_errors=list(errors)
pairs={}
for case in sorted({r['case'] for r in summary['runs']}):
    a=runs.get(case+'/off');b=runs.get(case+'/native')
    if not a or not b:
        errors.append('missing pair '+case);continue
    def signature(r):return [(h['user'],h['source'],h['intervals']) for h in r['holds']]
    equal=signature(a)==signature(b)
    users=sorted({h['user'] for r in (a,b) for h in r['holds']})
    per_user={str(u):[(h['source'],h['intervals']) for h in a['holds'] if h['user']==u]==
                     [(h['source'],h['intervals']) for h in b['holds'] if h['user']==u] for u in users}
    pairs[case]={'equal_complete_interval_sequences':equal,'off_holds':len(a['holds']),'native_holds':len(b['holds']),
                 'off_repeats':sum(len(h['repeats']) for h in a['holds']),'native_repeats':sum(len(h['repeats']) for h in b['holds']),
                 'per_user_sequences_equal':per_user}
    if not equal:errors.append('different full interval sequences '+case)
long_holds=[h for r in runs.values() for h in r['holds'] if h['source']>=16 and h['intervals'][:8]==[26,25,24,23,22,21,20,20]]
if not long_holds:errors.append('no complete direction acceleration sequence')
if not long_holds:policy_errors.append('no complete direction acceleration sequence')
result={'passed':not errors and len(summary['runs'])==8 and all(r['complete'] for r in summary['runs']),
        # Preserve the stricter full-stream check above. The task's delay /
        # interval criterion is reported separately: unchanged seconds CSVs
        # can deliver a different number/order of holds near a host tick.
        'policy_passed':not policy_errors and len(summary['runs'])==8 and all(r['complete'] for r in summary['runs']),
        'paired_policy_passed':not policy_errors and len(summary['runs'])==8 and len(pairs)==4 and all(r['complete'] for r in summary['runs']) and all(all(p['per_user_sequences_equal'].values()) for p in pairs.values()),
        'policy_errors':policy_errors,'errors':errors,'pairs':pairs,'runs':runs,'long_holds':len(long_holds),'sha256':sha,
        'replaced_initial_attempts':replaced,'coop_repeat':str(args.coop_repeat) if args.coop_repeat else None,'case_repeats':[str(p) for p in args.case_repeat]}
(args.output or root/'repeat-proof.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ('runs','sha256')},indent=2))
sys.exit(0 if result['passed'] else 1)
