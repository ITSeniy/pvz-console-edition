"""Audit the complete T15 evidence set without running or weakening checkers.

Inputs: final compare summary, all five render partitions, eight regression
workers (ON only for --amend), repeat proof, synthetic manifests and current EXE. Output: SHA-256
provenance plus explicit acceptance criteria in runtime-data/t15-acceptance.json.
Missing/incomplete/changed evidence fails. Run after all dispatched workers end.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import sys

root=Path(__file__).resolve().parents[1]
runtime=root/'runtime-data'
parser=argparse.ArgumentParser()
parser.add_argument('--amend',action='store_true',help='Audit acceptance follow-up, preserving the first evidence set')
parser.add_argument('--compare-root',type=Path,help='Completed resumed proof root, with its original reuse provenance')
parser.add_argument('--repeat-proof',type=Path,help='Explicit final repeat result, including rejected stronger equality checks')
args=parser.parse_args()
prefix='t15-amend' if args.amend else 't15-final'
criteria=[]
hashes={}
def sha(path):
    path=Path(path)
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    hashes[str(path)]=digest
    return digest
def read(path):
    sha(path)
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def require(name,condition,evidence=None):
    criteria.append({'name':name,'passed':bool(condition),'evidence':evidence})
def verify_file(path,digest):
    if sha(path)!=digest.lower():raise ValueError('changed evidence '+str(path))
exe=sha(root/'build/pvz-console-edition.exe')
cases={'adventure':5000,'versus':6000,'minigame':3000,'zen':5000,'menu':5000}
def audit_summary(directory):
    s=read(directory/'summary.json')
    require(str(directory.relative_to(root))+'/passed',s['passed'],s.get('failure'))
    require(str(directory.relative_to(root))+'/criteria',bool(s['criteria']) and all(c['passed'] for c in s['criteria']))
    require(str(directory.relative_to(root))+'/exe',s['inputs']['build/pvz-console-edition.exe']==exe)
    # Every phase and raw file is verified against the producing runner's SHA.
    for phase in s['phases'].values():
        verify_file(phase['path'],phase['sha256']);data=read(phase['path'])
        for run in data['runs'].values():
            for p,d in zip(run['log_files'],run.get('log_sha256',[])):verify_file(p,d)
            if 'dump' in run:verify_file(run['dump'],run['dump_sha256'])
            if 'render' in run:verify_file(run['render'],run['render_sha256'])
            receipt=read(run['receipt'])
            raw=receipt.get('raw_log_files',[])
            if isinstance(raw,dict):raw=[raw]
            for record in raw:verify_file(record['path'],record['sha256'])
            if 'receipt_sha256' in run:verify_file(run['receipt'],run['receipt_sha256'])
            if 'complete' in receipt:
                # ProfileOnly negative runs intentionally have no state dump;
                # their observed dump-frame count is 0, not a shortened replay.
                complete=receipt['complete'] and receipt['replay_complete']
                if 'dump' in run:complete=complete and receipt['frames']==receipt['frame_target']
                require(run['receipt']+'/complete',complete)
            if 'input_manifest' in receipt:verify_file(receipt['input_manifest'],receipt['input_sha256'])
    for key,digest in s['inputs'].items():
        if key=='scripts/run_native_proof.ps1':path=directory/'runner-input.ps1'
        elif key=='docs/native-port/data/native-replacements.csv':path=directory/'inventory-input.csv'
        elif key.startswith('baseline/'):path=root/'runtime-data/zen-shop-probe-user'/key[9:].lstrip('/\\')
        elif key.startswith('steps/'):path=root/'tests/replays/native-proof'/key[6:]
        elif key.startswith('supplemental/'):path=root/'tests/replays'/key[13:]
        else:path=root/key
        verify_file(path,digest)
    for b,p in s['synthetic'].items():
        verify_file(p['path'],p['sha256']);verify_file(p['source_manifest'],p['source_manifest_sha256'])
        verify_file(root/f'build/{b}_synthetic_test.exe',p['executable_sha256'])
        syn=read(p['path']);require(str(directory)+f'/synthetic/{b}',syn['passed'] and all(f['cases']>0 and f['errors']==0 and f['negative_errors']==f['cases'] for f in syn['functions']))
    return s

try:
    compare_root=args.compare_root.resolve() if args.compare_root else runtime/f'{prefix}-compare'
    compare=audit_summary(compare_root)
    require('compare/all_five_cases',set(compare['cases'])==set(cases))
    require('compare/repeat',all(p in compare['phases'] for p in ('original','compare','compare-repeat','native','negative','compare-extra','native-extra','negative-extra')))
    if args.amend:
        require('compare/all_three_batches',compare['batch']=='b001+b002+b003')
        state=compare['functions']['gamepad_82190D80']
        require('set_state/real_coverage',state['mode']=='SnapshotsOnly' and state['status']=='replaced' and state['coverage']>0 and state['errors']==0,state)
        syn=read(Path(compare['synthetic']['b003']['path']))
        field=next(f for f in syn['functions'] if f['address']=='0x82120000')
        require('field64/synthetic',len(syn['functions'])==9 and field['cases']==field['negative_errors']==1033 and field['errors']==0,field)
        inventory=list(csv.DictReader((compare_root/'inventory-input.csv').open(encoding='utf-8-sig')))
        selected=[r for r in inventory if r['batch'] in ('b002','b003')]
        counts={status:sum(r['status']==status for r in selected) for status in ('replaced','unproven','existing-hook')}
        require('inventory/37_9_7',counts=={'replaced':37,'unproven':9,'existing-hook':7},counts)
    neg_count=0
    for case in cases:
        s=audit_summary(runtime/f'{prefix}-render-{case}')
        require(f'render/{case}/case',s['cases']==[case])
        require(f'render/{case}/all_combinations',all(k in s['phases'] for k in ('render-original','render-repeat','render-b002','render-b003','render-b001+b002+b003')))
        require(f'render/{case}/comparisons',all(f'{group}/{case}' in s['render'] and s['render'][f'{group}/{case}']['equal'] for group in ('repeat','b002','b003','b001+b002+b003')))
        n=s['render']['negative']
        if n.get('first_difference'):
            neg_count+=1
            d=n['first_difference']
            require(f'render/{case}/negative',not n['equal'] and d['a']['scope']=='widget_82473730' and d['a']['op']=='8249B1A0' and d['b']['x']==d['a']['x']+1)
    require('render/caught_negative',neg_count>0,neg_count)
    regressions={}
    validation_names={'feedback':'feedback-regression-validation.json','multiplayer':'multiplayer-regression-validation.json',
                      'services':'services-feedback-validation.json','shop':'shop-regression-validation.json'}
    for mode in (('on',) if args.amend else ('off','on')):
        passed=total=0;regressions[mode]={}
        for suite,name in validation_names.items():
            dispatch=read(runtime/f't15-regression-{"amend" if args.amend else "final"}-{mode}-{suite}-processes.json')
            if len(dispatch)!=1:raise ValueError('unexpected regression dispatch')
            item=dispatch[0];worker=Path(item['worker']);receipt=read(worker/'runtime-data/receipt.json')
            verify_file(worker/'build/pvz-console-edition.exe',exe)
            require(f'regression/{mode}/{suite}/receipt',receipt['exit_code']==0,receipt)
            v=read(worker/'runtime-data'/name)
            require(f'regression/{mode}/{suite}/checks',v['passed']==v['total'] and v['total']>0 and all(v['checks'].values()) and not v['evidence_errors'],
                    {'passed':v['passed'],'total':v['total'],'evidence_errors':v['evidence_errors']})
            passed+=v['passed'];total+=v['total'];regressions[mode][suite]={'passed':v['passed'],'total':v['total']}
        require(f'regression/{mode}/276',passed==total==276,{'passed':passed,'total':total})
    repeat_path=args.repeat_proof.resolve() if args.repeat_proof else runtime/f'{prefix}-gamepad/repeat-proof-accepted.json'
    if not repeat_path.exists():repeat_path=runtime/f'{prefix}-gamepad/repeat-proof.json'
    repeat=read(repeat_path)
    for p,d in repeat['sha256'].items():verify_file(p,d)
    require('repeat/delays_and_intervals',repeat['policy_passed'],repeat['policy_errors'])
    require('repeat/paired_per_user_sequences',repeat['paired_policy_passed'],repeat['pairs'])
    # The stronger wall-clock event-stream comparison is retained verbatim;
    # it is not substituted for the task's logic-step delay/interval contract.
    gamepad=read(runtime/f'{prefix}-gamepad/summary.json')
    for r in gamepad['runs']:
        inputs=read(Path(r['receipt']).parent/'inputs.json')
        require(r['case']+'/'+r['mode']+'/same_exe',inputs['exe'].lower()==exe)
    ctest=(runtime/f'{prefix}-ctest.log').read_text(encoding='utf-8-sig')
    require('ctest/22', '100% tests passed, 0 tests failed out of 22' in ctest and 'Skipped' not in ctest)
    sha(runtime/f'{prefix}-ctest.log')
    control=read(runtime/'t15-master-control.json')
    require('master_control/head',control['head']=='a8d8ab3a0ea5e5d4418884573178ef550acc30e3')
    control_root=Path(control['root'])
    for p,d in control['source'].items():verify_file(control_root/p,d)
    verify_file(control_root/'build/pvz-console-edition.exe',control['exe_sha256'])
    require('master_control/reproduced',control['passed'],control.get('failure'))
    control_run=read(control['gamepad_summary'])
    require('master_control/field_coverage',len(control_run['runs'])==1 and control_run['runs'][0]['complete'] and control_run['runs'][0]['field_coverage'])
    cr=control_run['runs'][0]
    ci=read(Path(cr['receipt']).parent/'inputs.json')
    require('master_control/same_replay',ci['replay'].lower()==sha(root/'tests/replays/t07b_coop_hold_matrix.csv'))
    smoke=read(cr['receipt'])
    for p in smoke['log_files']:sha(p)
    if args.amend:
        rejected=read(runtime/'t15-amend-rejected.json')
        for p,d in rejected['sha256'].items():verify_file(p,d)
        require('rejected/preserved',bool(rejected['sha256']),len(rejected['sha256']))
        acceleration=read(runtime/'t15-amend-master-accel-checked.json')
        for p,d in acceleration['sha256'].items():verify_file(p,d)
        require('master_control/acceleration_intervals',not acceleration['policy_errors'] and
                len(acceleration['runs'])==1 and all(r['complete'] for r in acceleration['runs'].values()))
except Exception as e:
    require('evidence_complete_and_unchanged',False,str(e))
result={'passed':bool(criteria) and all(c['passed'] for c in criteria),'exe_sha256':exe,'criteria':criteria,'sha256':hashes}
if 'regressions' in locals():result['regressions']=regressions
if 'repeat' in locals():result['repeat_strict_full_stream_passed']=repeat['passed'];result['repeat_pairs']=repeat['pairs']
sha(Path(__file__))
(runtime/('t15-amend-acceptance.json' if args.amend else 't15-acceptance.json')).write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({'passed':result['passed'],'criteria':len(criteria),'failed':[c for c in criteria if not c['passed']]},indent=2))
sys.exit(0 if result['passed'] else 1)
