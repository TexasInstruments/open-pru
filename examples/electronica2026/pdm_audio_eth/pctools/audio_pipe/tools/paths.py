# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""SCENE_ROOT: the folder with the audio_wav* scenes (and the Scene Simulator checkout as scenes/).

Order: $SCENE_ROOT, else the folder that holds this crate. The second one follows the path the crate was reached by
(the shell's $PWD), so a crate symlinked into the scenes folder finds the scenes next to the link.
"""
import os


def _logical(p):
    """p as reached from the shell's $PWD (os.getcwd() has symlinks resolved, $PWD keeps them)."""
    p, pwd = os.path.abspath(p), os.environ.get("PWD", "")
    if pwd and os.path.realpath(pwd) == os.getcwd():
        return os.path.normpath(os.path.join(pwd, os.path.relpath(p, os.getcwd())))
    return p


CRATE = os.path.dirname(os.path.dirname(_logical(__file__)))
SCENE_ROOT = os.path.abspath(os.environ.get("SCENE_ROOT", os.path.join(CRATE, "..")))
