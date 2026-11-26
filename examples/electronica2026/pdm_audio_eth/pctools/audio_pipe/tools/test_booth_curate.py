# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
# python3 tools/test_booth_curate.py: checks booth_curate.py on made-up transcript logs
import json, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TALKERS = [("male", 30, "Hi there, how are you doing today?"), ("female", 150, "I'm good. What is the plan for the day?"),
           ("male_low", 210, "Please bring the red toolbox to station 4."), ("female_high", 270, "The conveyor belt stopped five minutes ago.")]


def playlist(d):
    sd = os.path.join(d, "factory_a")
    os.makedirs(sd)
    speakers = [{"id": f"s{i}", "voice": v, "az": az, "start": 0.3 + 0.4 * i} for i, (v, az, _) in enumerate(TALKERS)]
    json.dump({"speakers": speakers}, open(os.path.join(sd, "settings.json"), "w"))
    return d


def log(d, passes, change=None, extra=None):
    """passes 0..passes-1 of factory_a; change = (pass, voice, text), extra = (pass, az, text)"""
    p = os.path.join(d, "log.jsonl")
    with open(p, "w") as f:
        for n in range(passes):
            start = 1_000_000 + n * 168_000
            f.write(json.dumps({"mark": {"pass": n, "scene": "factory_a", "start": start}}) + "\n")
            for k, (v, az, text) in enumerate(TALKERS):
                if change and change[0] == n and change[1] == v:
                    text = change[2]
                f.write(json.dumps({"pass": n, "scene": "factory_a", "speaker": k, "az": az + 1.5, "start": start + 5000,
                                    "end": start + 40000, "text": text, "latency_ms": 520.0}) + "\n")
            if extra and extra[0] == n:
                f.write(json.dumps({"pass": n, "scene": "factory_a", "speaker": 9, "az": extra[1], "start": start,
                                    "end": start + 9000, "text": extra[2], "latency_ms": 500.0}) + "\n")
        # the pass running when the log ended (cut off, not scored)
        f.write(json.dumps({"mark": {"pass": passes, "scene": "factory_a", "start": 1_000_000 + passes * 168_000}}) + "\n")
    return p


def curate(logp, pl):
    r = subprocess.run([sys.executable, os.path.join(HERE, "booth_curate.py"), logp, pl], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


with tempfile.TemporaryDirectory() as d:
    pl = playlist(os.path.join(d, "pl"))
    code, out = curate(log(d, 5), pl)
    assert code == 0 and "factory_a: PASS" in out, out

    code, out = curate(log(d, 5, change=(2, "female_high", "Their belt stopped five minutes ago.")), pl)
    assert code != 0 and "factory_a: FAIL" in out, out
    assert "female_high" in out and "pass 2" in out, out

    code, out = curate(log(d, 5, extra=(3, 170, "I don't know.")), pl)
    assert code != 0 and "factory_a: FAIL" in out and "extra text" in out, out

    # only the last 5 passes count: an early bad pass is fine after 5 good ones
    code, out = curate(log(d, 6, change=(0, "male", "Hi there.")), pl)
    assert code == 0 and "factory_a: PASS" in out, out

    code, out = curate(log(d, 4), pl)
    assert code != 0 and "4 of 5" in out, out
print("test_booth_curate: ok")
