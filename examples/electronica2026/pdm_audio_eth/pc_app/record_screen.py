#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Record the whole screen together with what plays on the speakers, into one MP4. No dialog.

  record-screen [seconds=10] [out.mp4]          (default ~/Videos/demo_<date>_<time>.mp4)

Install once (as pdm-eth-demo): ln -sf "$(readlink -f record_screen.py)" ~/.local/bin/record-screen
Needs GNOME Shell on Wayland (tested with GNOME Shell 50.1), GStreamer with gst-plugins-good/bad/ugly and gst-libav, wpctl.
The GNOME screenshot tool's recorder has no sound; this one has.

Uses GNOME Shell's own screencast service (the one behind the screenshot tool's recorder), with a custom
pipeline: the screen video plus the monitor of the default audio output (everything you hear), muxed in the same
GStreamer pipeline, so picture and sound stay in sync. H.264 (NVENC, else x264) + AAC 192 kbit/s, 30 fps.
The recording belongs to this process's D-Bus connection: GNOME stops it if the process exits early.
"""
import os, re, subprocess, sys, time
import gi
gi.require_version("Gst", "1.0")
from gi.repository import Gio, GLib, Gst

secs = float(sys.argv[1]) if len(sys.argv) > 1 else 10.0
out = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else
                      time.strftime(os.path.expanduser("~/Videos/demo_%Y%m%d_%H%M%S.mp4")))
os.makedirs(os.path.dirname(out), exist_ok=True)

sink = subprocess.run(["wpctl", "inspect", "@DEFAULT_AUDIO_SINK@"], capture_output=True, text=True).stdout
mon = re.search(r'node\.name = "([^"]+)"', sink).group(1) + ".monitor"
Gst.init(None)
venc = ("nvh264enc preset=hq bitrate=12000" if Gst.ElementFactory.find("nvh264enc")
        else "x264enc speed-preset=veryfast bitrate=12000")
pipeline = (f"videoconvert ! video/x-raw,format=NV12 ! queue ! {venc} ! h264parse ! queue ! mux. "
            f"pulsesrc device={mon} do-timestamp=true ! audioconvert ! audioresample ! audio/x-raw,rate=48000,channels=2 "
            f"! queue ! avenc_aac bitrate=192000 ! aacparse ! queue ! mux. "
            f"mp4mux name=mux faststart=true")

bus = Gio.bus_get_sync(Gio.BusType.SESSION)
SC = ("org.gnome.Shell.Screencast", "/org/gnome/Shell/Screencast", "org.gnome.Shell.Screencast")
opts = {"pipeline": GLib.Variant("s", pipeline), "framerate": GLib.Variant("i", 30),
        "draw-cursor": GLib.Variant("b", True)}
stem = out[:-4] if out.endswith(".mp4") else out  # GNOME appends its own extension ("undefined" for a custom pipeline)
ok, used = bus.call_sync(*SC, "Screencast", GLib.Variant("(sa{sv})", (stem, opts)), None, 0, 10000, None).unpack()
if not ok:
    sys.exit("GNOME refused to start the screencast")
print(f"recording {secs:.0f} s (audio: {mon})")
time.sleep(secs)
ok = bus.call_sync(*SC, "StopScreencast", None, None, 0, 30000, None).unpack()[0]
time.sleep(1.0)  # let the muxer finish the file
if os.path.exists(used) and used != out:
    os.replace(used, out)
print(f"saved {out}" if ok else f"stop failed, file may be incomplete: {out}")
