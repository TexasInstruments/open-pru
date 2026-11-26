#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
#
# check_icssg0_copy.sh - the ICSSG0 PDM firmware of pdm_audio_eth must stay
# byte-identical to the board-tested pdm_audio project. Exit 0 if all files
# match, 1 otherwise. Run from anywhere.

here=$(cd "$(dirname "$0")/.." && pwd)
ref="$here/../../pdm_audio/pdm_audio"
status=0
for f in firmware/rtu1_mod.asm firmware/pru1_pdm.asm firmware/pru0_rx.asm \
         firmware/pdm8_shared.inc firmware/fir_coefs.inc \
         firmware/decimation_fir.h mcuplus/pdm8_shared.h; do
    if cmp -s "$ref/$f" "$here/$f"; then
        echo "same    $f"
    else
        echo "DIFFERS $f"
        status=1
    fi
done
exit $status
