# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
# python3 eval.py <tag> [extra args...]: runs audio_pipe on test scenes, scores per-speaker WER
import json, subprocess, sys, re, os, math
SP=os.environ.get('TESTDATA', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'testdata'))
BIN=os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'target', 'release', 'audio_pipe')
REF={'male':'hi there how are you doing today','female':"i'm good what is the plan for the day"}
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from paths import SCENE_ROOT as UP
SCENES={'example':(os.path.join(UP,'audio_wav'),[('male',20),('female',150)])}
for s in ['home','factory','hospital','automobile','quiet','robot']:
    st=json.load(open(f'{SP}/scene_{s}/settings.json'))
    SCENES[s]=(f'{SP}/scene_{s}',[(sp['voice'],sp['az']) for sp in st['speakers'] if sp['voice'] in REF])
def norm(t): t=t.lower().replace("what's","what is"); return re.sub(r"[^a-z' ]"," ",t.lower()).split()
def wer(ref,hyp):
    r,h=norm(ref),norm(hyp); d=list(range(len(h)+1))
    for i in range(1,len(r)+1):
        nd=[i]+[0]*len(h)
        for j in range(1,len(h)+1): nd[j]=min(d[j]+1,nd[j-1]+1,d[j-1]+(r[i-1]!=h[j-1]))
        d=nd
    return d[-1]/len(r)
def angd(a,b): return abs((a-b+180)%360-180)
tag=sys.argv[1]; extra=sys.argv[2:]
only=os.environ.get('SCENES')
tot=[];
for name,(d,spk) in SCENES.items():
    if only and name not in only.split(','): continue
    out=f"{SP}/ev_{tag}_{name}"
    r=subprocess.run([BIN,'run',d,'--out',out]+extra,capture_output=True,text=True)
    if r.returncode: print(name,'FAILED',r.stderr[-500:]); continue
    tr=json.load(open(f'{out}/transcript.json'))
    dsp=re.search(r'per-frame ms: mean ([\d.]+).*?p99 ([\d.]+).*?max ([\d.]+)',r.stderr)
    used=set(); line=[]
    for v,az in spk:
        c=[t for t in tr if angd(t['azimuth_deg'],az)<=20]
        c.sort(key=lambda t:angd(t['azimuth_deg'],az))
        txt=c[0]['text'] if c else ''
        if c: used.add(c[0]['speaker'])
        w=wer(REF[v],txt); tot.append(w)
        line.append(f"{v}@{az}->{'%.0f'%c[0]['azimuth_deg'] if c else 'MISS'} WER {w:.2f} \"{txt[:70]}\"")
    extras=[t for t in tr if t['speaker'] not in used and t['text'].strip()]
    print(f"{name:10s} {' | '.join(line)}  | extra-text-streams {len(extras)} | frame ms mean {dsp.group(1)} max {dsp.group(3)}")
    for e in extras: print(f"{'':10s}   extra spk{e['speaker']}@{e['azimuth_deg']:.0f}: \"{e['text'][:80]}\"")
print(f'== {tag}: mean WER {sum(tot)/len(tot):.3f} over {len(tot)} talkers')
