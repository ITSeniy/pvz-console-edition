"""Strict T14 JSONL draw comparison. Input/output SHA-256, first step/call diff.

No tolerance, pointer rewriting, omitted steps or truncated frames are accepted.
The trace producer selects one complete draw after each logical update.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile

def read(path):
    records = [json.loads(line) for line in path.read_text().splitlines()]
    if len(records)<3 or records[0].get('schema')!=1 or records[-1].get('complete') is not True:
        raise ValueError(f'incomplete trace: {path}')
    frames=records[1:-1]
    if len(frames)!=records[-1]['frames']:
        raise ValueError('frame count does not match footer')
    for i,frame in enumerate(frames):
        if frame.get('step')!=i+1 or not isinstance(frame.get('calls'),list):
            raise ValueError(f'missing/duplicate logical step: {i+1}')
    return records[0],frames

def compare(a,b):
    ah,af=read(a);bh,bf=read(b)
    result={'equal':True,'a_sha256':hashlib.sha256(a.read_bytes()).hexdigest(),
            'b_sha256':hashlib.sha256(b.read_bytes()).hexdigest(),
            'frames_a':len(af),'frames_b':len(bf),'calls_a':sum(len(f['calls']) for f in af),
            'calls_b':sum(len(f['calls']) for f in bf),'first_difference':None}
    if ah!=bh:
        result.update(equal=False,first_difference={'header':True})
        return result
    for i in range(max(len(af),len(bf))):
        ac=af[i]['calls'] if i<len(af) else None
        bc=bf[i]['calls'] if i<len(bf) else None
        if ac==bc:
            continue
        j=next((j for j in range(min(len(ac or []),len(bc or []))) if ac[j]!=bc[j]),min(len(ac or []),len(bc or [])))
        result.update(equal=False,first_difference={'step':i+1,'call':j,
            'a':ac[j] if ac and j<len(ac) else None,'b':bc[j] if bc and j<len(bc) else None})
        break
    return result

def self_test():
    with tempfile.TemporaryDirectory() as folder:
        a,b=Path(folder)/'a',Path(folder)/'b'
        data=[{'schema':1},{'step':1,'calls':[{'op':'8249B1A0','x':10}]},{'complete':True,'frames':1}]
        def write(p,rows):p.write_text('\n'.join(map(json.dumps,rows)))
        write(a,data);write(b,data);assert compare(a,b)['equal']
        data[1]['calls'][0]['x']=11;write(b,data)
        r=compare(a,b);assert not r['equal'] and r['first_difference']['step']==1 and r['first_difference']['call']==0
        for rows in (data[:-1],[data[0],dict(step=2,calls=[]),data[-1]],data+[data[-1]]):
            write(b,rows)
            try:compare(a,b)
            except (ValueError,KeyError):pass
            else:raise AssertionError('invalid trace accepted')
    print('render trace parser: identity, one-pixel error, truncation, missing step, duplicate footer passed')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('a',nargs='?',type=Path);p.add_argument('b',nargs='?',type=Path)
    p.add_argument('--output',type=Path);p.add_argument('--self-test',action='store_true');args=p.parse_args()
    if args.self_test:self_test()
    else:
        try:r=compare(args.a,args.b)
        except (ValueError,KeyError) as e:r={'equal':False,'error':str(e)}
        text=json.dumps(r,indent=2)
        if args.output:args.output.write_text(text)
        print(text);raise SystemExit(not r['equal'])
