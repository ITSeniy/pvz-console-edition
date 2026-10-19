"""Reproduce T15 inventory/layout evidence; optionally incorporate a proof JSON.

Inputs: t15_xbox_evidence manifest, current declarations, synthetic/proof results.
Outputs: only the two authorized CSV tables. Does not promote unobserved code.
Run from repository root: python -I tests/t15_update_inventory.py [proof-summary].
"""
import csv
import hashlib
import json
from pathlib import Path
import re
import sys
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'runtime-data/t15-evidence/manifest.json').read_text())
proof=json.loads(Path(sys.argv[1]).read_text(encoding='utf-8-sig')) if len(sys.argv)>1 else None
hooks={
 '82189B40':'service_mouse.cpp:686: указатель Zen/Tree при владении мышью; иначе original',
 '82580A78':'mouse_bridge.cpp:3338: held B/grab для лопаты/butter/Beghouled по Xbox LR; иначе original',
 '82580C58':'mouse_bridge.cpp:2508: направление стика X для мыши в Heavy Weapon',
 '82580C80':'mouse_bridge.cpp:2656: направление стика Y для мыши в Heavy Weapon',
 '82580D88':'gamepad_repeat.cpp:123: дословное тело с repeat-loop T07b один tick на логическое обновление; CTest gamepad_repeat_body',
 '82492618':'state_dump.cpp:200: root WidgetManager original update затем снимок/профиль; вложенные вызовы original',
 '82492A48':'mouse_bridge.cpp:3538: адаптация отрисовки/клиппинга мыши; существующий hook сохранён',
}
abi={
 '82120000':'r3=self,r4=value; word self+64=r4.u32; no calls/register outputs',
 '82190D80':'r3=self,r4=new state; frame70; virtual+18 then state+68=new and virtual+14(previous); if changed float self+70=image820B5244; SnapshotsOnly callbacks',
 '82580A38':'r3=self; r3.u64=zero-extended byte self+C; no stores/calls',
 '82580A68':'f1.f64=double(image float 82000A94); no stores/calls',
 '82580AD8':'r3=self,r1=SP; f1.f64=rounded float(signed self+10)*image float 8201827C; signed 64-bit store entry.SP-16..-9; no calls',
 '82580B00':'r3=self,r1=SP; f1.f64=rounded float(signed self+14)*image float 8201827C; signed 64-bit store entry.SP-16..-9; no calls',
 '82580CA8':'r3=self; f1/f2/f3 doubles; f1 clamped only when > image 82000B80; stores self+18C..19B floats; NaN branch order preserved; no calls',
 '82491CA8':'r3=self; word self+38 increment modulo 2^32; r3 preserved; no calls',
 '82491CF0':'r3=out,r4=self; load self+40..4F before four stores out..out+15; r3.u64 identity; aliases allowed; no calls',
 '821898C0':'r3=self,r4=Board*,r5=player,r6=user; constructor; self+0..AF fields plus listener vector allocation/registration; frame B0; SnapshotsOnly',
 '82189A58':'r3=self,r4=delete flags; listener vtable at self+4, free self+14/self+8 and optional self; frame70; SnapshotsOnly',
 '82189B28':'r3=controls; tail-dispatch Board+1D4 Challenge; r3=0 if absent; descendants have board/IO effects; SnapshotsOnly',
 '82190048':'r3=self,r4=new state; state 1..6 entry effects, coordinates and callbacks; frame A0; SnapshotsOnly',
 '82190408':'r3=self; state2 clears +84/+88; state6 elapsed>1 calls Board action 6; frame70; SnapshotsOnly',
 '821905F8':'r3=self,f1=dt; +70 elapsed; state1..7 vector/delta/acceleration/target fields; virtual pad input and state transitions; frameC0; SnapshotsOnly',
 '82190DE0':'r3=out,r4=controls; out8 predicted cell; Board pixel/grid helpers; FMA/float truncation; frame90; SnapshotsOnly',
 '82190ED8':'r3=out,r4=controls; out8 cell anchor; virtual+28 and Board/Zen helpers; frame80; SnapshotsOnly',
 '82192C10':'r3 -=4; tail-call 82189A58; destructor thunk; SnapshotsOnly',
 '825808E8':'r3=self,r4=user; constructor and 82580870 state init; self fields C..198 and button mapping; frame70; SnapshotsOnly',
 '825809E8':'r3=self; vtable reset and XInput vibration IO with stack zero vibration; frame60; SnapshotsOnly',
 '82581898':'r3=self,r4=flags; dispatched destructor then optional guest free; returns self; frame70; SnapshotsOnly',
 '82580A90':'r3=self,r4=button; virtual+18 boolean mapped to f1=image 0/1; frame60; callback effects unknown; SnapshotsOnly',
 '82580B28':'r3=self; signed axes +10/+14, deadzone image8207DAC0, rounded length; 82166AE0 normalize; f1=X; frame70; SnapshotsOnly scratch stack',
 '82580BC0':'r3=self; signed axes +10/+14, deadzone image8207DAC0, rounded length; 82166AE0 normalize; f1=Y; frame70; SnapshotsOnly scratch stack',
 '82491C48':'r3=self; parent+24 virtual+C(self) or byte self+3C=1; tail; SnapshotsOnly callbacks',
 '82491C78':'r3=self; parent+24 virtual+14(self) or byte self+3C=1; tail; SnapshotsOnly callbacks',
 '82491D18':'r3=self,r4=other; two virtual GetRect results on frame90 then rectangle intersection bool r3; SnapshotsOnly callbacks',
 '824922F0':'r3=self,r4=manager; checked list forward; child+20=manager, virtual+1C; parent dirty; frame80; SnapshotsOnly',
 '824923C8':'r3=self,r4=manager; checked list forward; disable child, virtual+20, child+20=0; clear manager+80 if self; frame90; SnapshotsOnly',
 '824924B0':'r3=self,r4=child; dirty propagation self/child and visible siblings after child; virtual intersection; frame90; SnapshotsOnly',
 '82492850':'r3=self,r4=ModalFlags*,f1=dt; temporarily modify flags0/4; context+8 modal bit; self+28/+2C/+30 mutable iterator; virtual fractional updates; frameC0; SnapshotsOnly',
 '82492E40':'r3=self; virtual+4C; global828924D0 increment if children; children virtual+48; frame70; SnapshotsOnly callbacks',
 '82492F28':'r3=self; list ctor allocation then container fields0..67 incl iterator proxy/sentinel; frame70; SnapshotsOnly',
 '82492FD8':'r3=self; vtable reset; list destruction and free sentinel/proxy; self+18=0; frame60; SnapshotsOnly',
 '824931E8':'r3=self,r4=child; dirty-full self/child; checked backward/forward siblings; clip rect intersection and Contains callbacks; frameE0; SnapshotsOnly',
 '824940C0':'r3=self,r4=flags; dispatched container destructor then optional guest free; returns self; frame70; SnapshotsOnly',
}
inventory=root/'docs/native-port/data/native-replacements.csv'
with inventory.open(newline='',encoding='utf-8-sig') as f:
    old=list(csv.DictReader(f));fields=list(old[0])
rows=[r for r in old if r['batch'] not in ('b002','b003')]
for batch,source in [('b002','b002_gamepad.cpp'),('b003','b003_widget_container.cpp')]:
    text=(root/'src/native'/source).read_text()
    # Key by address (the first captured group is the kind).
    declarations={m[2]:(m[1],m[3]) for m in re.finditer(r'^(LEAF|SNAP)\(([0-9A-F]{8}), (true|false),',text,re.M)}
    synth_path=root/f'build/{batch}_synthetic.json'
    if not synth_path.exists():synth_path=root/f'runtime-data/t15-{batch}-synthetic1.json'
    synthetic={r['address']:r for r in json.loads(synth_path.read_text())['functions']}
    for address,facts in manifest['functions'].items():
        if facts['batch'].lower()!=batch:continue
        r={k:'' for k in fields};r.update(address='0x'+address,batch=batch,name=facts['name'],
            instructions=facts['instructions'],children='|'.join(c for c in facts['children'] if c!='sub_'+address),
            evidence='verified Xbox; image SHA256='+manifest['sha256']['.cache/pvz-image.bin']+'; '+facts['generated']+' SHA256='+manifest['sha256'][facts['generated']])
        if address in hooks:
            r.update(status='existing-hook',mode='existing-hook',proof_kind='existing-hook',reason=hooks[address],abi='Excluded by T15; original port path retained')
        else:
            kind,proven=declarations[address];flag=('gamepad_' if batch=='b002' else 'container_')+address
            r.update(flag=flag,mode='DeclaredOutputsOnly' if kind=='LEAF' else 'SnapshotsOnly',status='replaced' if proven=='true' else 'unproven',
                     abi=abi.get(address,'blr/constant; r3.u64 identity or zero; all memory preserved; no children'),
                     proof_kind='synthetic' if kind=='LEAF' else 'snapshots',coverage=0,errors=0,
                     reason='Синтетика против дословного generated; игровой охват отдельно' if kind=='LEAF' else 'Кандидат; до ненулевого охвата и строгих снимков в пакет не допущен')
            if '0x'+address in synthetic:
                s=synthetic['0x'+address];r.update(synthetic_cases=s['cases'],synthetic_errors=s['errors'],synthetic_negative_errors=s['negative_errors'],
                    synthetic_domain='tests/batch_synthetic.h; declaration '+re.search(r'^LEAF\('+address+r'.*$',text,re.M)[0])
            if proof and flag in proof['functions']:
                p=proof['functions'][flag]
                for k in ('coverage','errors','negative_coverage','negative_errors'):r[k]=p[k]
                r['evidence']+='; proof='+str(Path(sys.argv[1]))+' SHA256='+hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest()
                if int(p['coverage'])>0:r['reason']='Ненулевой игровой охват; результаты строгих снимков/трасс и статус приёмки — в T15.md'
        rows.append(r)
with inventory.open('w',newline='',encoding='utf-8') as f:
    w=csv.DictWriter(f,fields);w.writeheader();w.writerows(rows)
layout=root/'docs/native-port/data/xbox-layouts.csv'
with layout.open(newline='',encoding='utf-8-sig') as f:reader=csv.DictReader(f);lf=reader.fieldnames;lr=list(reader)
lr=[r for r in lr if r['class'] not in ('Sexy::Gamepad','Sexy::WidgetContainer','Sexy::WidgetListNode','Sexy::WidgetUpdateContext')]
for cls,entries in [
 ('Sexy::Gamepad',[(n,o,t,a) for n,o,t,a in [
  ('vtable',0,'GuestPtr','82580910'),('user_index',4,'rex::be<uint32_t>','8258091C'),('field_08',8,'rex::be<uint32_t>','82580984'),
  ('connected',12,'uint8_t','82580A38'),('axis_x',16,'rex::be<int32_t>','82580AD8'),('axis_y',20,'rex::be<int32_t>','82580B00'),('button_map',316,'rex::be<uint32_t>[20]','82580940'),
  ('vibration_left',396,'rex::be<float>','82580CBC'),('vibration_right',400,'rex::be<float>','82580CCC'),('vibration_duration',404,'rex::be<float>','82580CD0'),('vibration_remaining',408,'rex::be<float>','82580CD4')]]),
 ('Sexy::WidgetContainer',[(n,o,t,a) for n,o,t,a in [
  ('vtable',0,'GuestPtr','82492F48'),('list_proxy',4,'GuestPtr','82492900'),('children_head',24,'GuestPtr','824928F8'),('children_count',28,'rex::be<uint32_t>','82492E60'),
  ('manager',32,'GuestPtr','82492980'),('parent',36,'GuestPtr','82491C48'),('iterator_changed',40,'uint8_t','82492954'),('iterator_owner',44,'GuestPtr','82492920'),('iterator_node',48,'GuestPtr','82492944'),
  ('last_manager_update_count',52,'rex::be<uint32_t>','82492FA0'),('update_count',56,'rex::be<uint32_t>','82491CA8'),('dirty',60,'uint8_t','82491C70'),
  ('x',64,'rex::be<int32_t>','82491CF0'),('y',68,'rex::be<int32_t>','82491CF4'),('width',72,'rex::be<int32_t>','82491CF8'),('height',76,'rex::be<int32_t>','82491CFC'),
  ('flag_50',80,'uint8_t','824924F4'),('flag_51',81,'uint8_t','82492FB0'),('flags_add',84,'rex::be<uint32_t>','8249286C'),('flags_remove',88,'rex::be<uint32_t>','82492878'),
  ('field_5c',92,'rex::be<uint32_t>','82492FB4'),('field_60',96,'rex::be<uint32_t>','82492FB8'),('field_64',100,'rex::be<uint32_t>','82492FBC')]]),
 ('Sexy::WidgetListNode',[('next',0,'GuestPtr','824929E0'),('previous',4,'GuestPtr','82493434'),('widget',8,'GuestPtr','82492984')]),
 ('Sexy::WidgetUpdateContext',[('modal_flags',0,'rex::be<uint32_t>','82492864'),('flags',4,'rex::be<uint32_t>','82492880'),('is_modal',8,'uint8_t','824928C0')])]:
    for name,offset,typ,address in entries:lr.append(dict(zip(lf,[cls,name,hex(offset),typ,'0x'+address,'T15 complete Xbox body','verified Xbox access'])))
with layout.open('w',newline='',encoding='utf-8') as f:w=csv.DictWriter(f,lf);w.writeheader();w.writerows(lr)
print('T15 rows:',sum(r['batch'] in ('b002','b003') for r in rows))
