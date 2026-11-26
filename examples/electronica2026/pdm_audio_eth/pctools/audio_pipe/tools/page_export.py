#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Export Scene Simulator scenes headless, as the page makes them (Kokoro typed text included).

    page_export.py OUT JOBS.json [--page ~/GitHub/scenes/web/robot_acoustic.html]

JOBS.json: a list of jobs
    {"name": "v_a", "scene": "factory_booth",
     "text":   {"s1": "Hi there, how are you doing today?", ...},   # optional, per speaker id
     "voices": {"male": "am_michael", ...}}                         # optional Kokoro voice per slot

Each job is rendered in a fresh page and its zip ("Download .zip") is written as OUT/<name>/mic1..8.wav +
settings.json (flat, the layout of pdm-eth gui --playlist) and OUT/<name>.zip. The browser profile is kept in
OUT/.chrome so the Kokoro model is downloaded only once. Needs playwright (python) and Chrome
(CHROMIUM=/path, default /usr/bin/google-chrome).
"""
import argparse, functools, http.server, json, os, shutil, socketserver, sys, tempfile, threading, zipfile

from playwright.sync_api import sync_playwright


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out")
    ap.add_argument("jobs")
    ap.add_argument("--page", default=os.path.expanduser("~/GitHub/scenes/web/robot_acoustic.html"))
    a = ap.parse_args()
    jobs = json.load(open(a.jobs))
    os.makedirs(a.out, exist_ok=True)
    site = tempfile.mkdtemp()
    shutil.copy(a.page, os.path.join(site, "index.html"))
    shutil.copytree(os.path.join(os.path.dirname(os.path.abspath(a.page)), "vendor"), os.path.join(site, "vendor"))

    class Q(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *x):
            pass

    srv = socketserver.ThreadingTCPServer(("127.0.0.1", 0), functools.partial(Q, directory=site))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{srv.server_address[1]}/index.html"
    chrome = os.environ.get("CHROMIUM", "/usr/bin/google-chrome")
    bad = 0
    with sync_playwright() as p:
        ctx = p.chromium.launch_persistent_context(os.path.join(a.out, ".chrome"), executable_path=chrome, accept_downloads=True)
        for j in jobs:
            pg = ctx.new_page()
            errs = []
            pg.on("pageerror", lambda e: errs.append(str(e)))
            pg.route("**/fonts.g*/**", lambda r: r.abort())
            pg.goto(url)

            def settled():
                pg.wait_for_timeout(700)
                pg.wait_for_function("__ui.RP && JSON.stringify(__ui.RP)===JSON.stringify(__ui.P) && /^Rendered/.test(document.querySelector('#status').textContent)",
                                     timeout=600000)

            settled()
            if j.get("voices"):
                pg.evaluate("v => Object.assign(__ui.KOKORO_VOICE, v)", j["voices"])
            pg.click(f"#card_{j.get('scene', 'factory_booth')}")
            settled()
            for sid, text in (j.get("text") or {}).items():
                pg.fill(f"#k_{sid}_text", text)
            settled()
            warn = pg.evaluate("__ui.RES.warnings")
            with pg.expect_download() as dl:
                pg.click("#dlAll")
            zp = os.path.join(a.out, j["name"] + ".zip")
            dl.value.save_as(zp)
            d = os.path.join(a.out, j["name"])
            shutil.rmtree(d, ignore_errors=True)
            os.makedirs(d)
            z = zipfile.ZipFile(zp)
            for n in z.namelist():
                if n.startswith("mics/mic") or n == "settings.json":
                    open(os.path.join(d, os.path.basename(n)), "wb").write(z.read(n))
            st = json.load(open(os.path.join(d, "settings.json")))
            said = [(s["voice"], s["az"], s.get("engine"), s["text"]) for s in st["speakers"]]
            ok = not warn and not errs and all(s["speech"] == "text" for s in st["speakers"])
            bad += not ok
            print(("ok  " if ok else "BAD ") + j["name"], said, warn or "", errs[:2] or "", flush=True)
            pg.close()
        ctx.close()
    srv.shutdown()
    shutil.rmtree(site, ignore_errors=True)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
