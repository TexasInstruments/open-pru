/*
 * Copyright (C) 2024-2025 Texas Instruments Incorporated
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   distribution and/or other materials provided with the
 *   distribution.
 *
 *   Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

let common = system.getScript("/common");
let pru_i2s_module_name = "/pru_i2s/pru_i2s";
let device = common.getDeviceName();
let is_am26x_soc = (device === "am263x-cc" || device === "am261x-lp" || device === "am263px-cc") ? true : false;
let is_am263x_soc = (device === "am263x-cc") ? true : false;
let is_am261x_soc = (device === "am261x-lp") ? true : false;
let is_am263px_soc = (device === "am263px-cc") ? true : false;

function onValidate(inst, validation) {
    /* Validate that at least one channel (Tx or Rx) is enabled */
    if (inst.Num_Tx_Channels === 0 && inst.Num_Rx_Channels === 0) {
        validation.logError("At least one Tx or Rx channel must be enabled", inst, "Num_Tx_Channels");
    }

    /* Validate channel limits */
    if (inst.Num_Tx_Channels > 3) {
        validation.logError("Maximum 3 Tx channels supported", inst, "Num_Tx_Channels");
    }

    if (inst.Num_Rx_Channels > 2) {
        validation.logError("Maximum 2 Rx channels supported", inst, "Num_Rx_Channels");
    }

    /* Validate TDM mode requirements */
    if (inst.Mode === "TDM") {
        let total_channels = inst.Num_Tx_Channels + inst.Num_Rx_Channels;
        if (total_channels > 4) {
            validation.logWarning(
                "TDM mode with " + total_channels + " channels may require special firmware configuration",
                inst,
                "Mode"
            );
        }
    }

    /* Validate sampling frequency for high channel counts */
    if ((inst.Num_Tx_Channels + inst.Num_Rx_Channels) > 2 &&
        parseInt(inst.Sampling_Freq) > 48) {
        validation.logWarning(
            "Sampling frequencies above 48 kHz with multiple channels may have performance limitations",
            inst,
            "Sampling_Freq"
        );
    }
}

let pru_i2s_module = {
    displayName: "PRU I2S Audio Interface",
    templates: {
        "/drivers/system/system_config.c.xdt": {
            driver_config: "/pru_i2s/pru_i2s_templates.c.xdt",
            moduleName: pru_i2s_module_name,
        },
        "/drivers/system/system_config.h.xdt": {
            driver_config: "/pru_i2s/pru_i2s_templates.h.xdt",
            moduleName: pru_i2s_module_name,
        },
        "/drivers/pinmux/pinmux_config.c.xdt": {
            moduleName: pru_i2s_module_name,
        },
    },
    defaultInstanceName: "CONFIG_PRU_I2S",
    config: [
        {
            name: "instance",
            displayName: "ICSS Instance",
            description: "Select the ICSS (Industrial Communication SubSystem) instance",
            default: (is_am261x_soc) ? "ICSSM1" : "ICSSM0",
            options: (is_am261x_soc) ?
                [
                    {
                        name: "ICSSM0",
                        displayName: "ICSSM0"
                    },
                    {
                        name: "ICSSM1",
                        displayName: "ICSSM1"
                    }
                ]
                :
                (is_am263x_soc || is_am263px_soc) ?
                [
                    {
                        name: "ICSSM",
                        displayName: "ICSSM0"
                    }
                ]
                :
                [
                    {
                        name: "ICSSG0",
                    },
                    {
                        name: "ICSSG1",
                    }
                ]
        },
        {
            name: "PRU_Slice",
            displayName: "PRU Core",
            description: "Select PRU core (slice) for I2S operation",
            default: "PRU0",
            options: [
                {
                    name: "PRU0",
                    displayName: "PRU0"
                },
                {
                    name: "PRU1",
                    displayName: "PRU1"
                },
            ],
        },
        {
            name: "Mode",
            displayName: "I2S/TDM Mode",
            description: "Select I2S (standard stereo) or TDM (multi-channel time division multiplexing) mode",
            longDescription: `
**I2S Mode**: Standard Inter-IC Sound protocol for stereo audio
- Typically 2 channels (left/right)
- Standard for audio codecs and DACs
- Lower latency

**TDM Mode**: Time Division Multiplexing for multi-channel audio
- Supports up to 4+ channels
- Used in multi-microphone arrays and multi-speaker systems
- Higher throughput
            `,
            default: "I2S",
            options: [
                {
                    name: "I2S",
                    displayName: "I2S Mode (Standard Stereo)"
                },
                {
                    name: "TDM",
                    displayName: "TDM Mode (Multi-Channel)"
                },
            ],
        },
        {
            name: "Num_Tx_Channels",
            displayName: "Number of Tx Channels",
            description: "Number of transmit (output) I2S channels (0-3)",
            longDescription: `
Specify the number of I2S transmit channels:
- **0**: No transmit channels (Rx only configuration)
- **1**: Single Tx channel (mono or single stereo pair)
- **2**: Two Tx channels (dual stereo pairs)
- **3**: Three Tx channels (6 audio channels in TDM)

Common configurations:
- Tx=1, Rx=0: Audio output only (DAC)
- Tx=0, Rx=1: Audio input only (ADC/microphone)
- Tx=1, Rx=1: Full-duplex audio (codec)
            `,
            default: 1,
            options: [
                { name: 0, displayName: "0 (Rx only)" },
                { name: 1, displayName: "1" },
                { name: 2, displayName: "2" },
                { name: 3, displayName: "3 (TDM)" },
            ],
        },
        {
            name: "Num_Rx_Channels",
            displayName: "Number of Rx Channels",
            description: "Number of receive (input) I2S channels (0-2)",
            longDescription: `
Specify the number of I2S receive channels:
- **0**: No receive channels (Tx only configuration)
- **1**: Single Rx channel (mono or single stereo pair)
- **2**: Two Rx channels (dual stereo pairs / 4-mic array)

Common configurations:
- Tx=1, Rx=0: Audio output only (DAC)
- Tx=0, Rx=1: Audio input only (ADC/microphone)
- Tx=1, Rx=1: Full-duplex audio (codec)
- Tx=0, Rx=2: Stereo microphone array
            `,
            default: 0,
            options: [
                { name: 0, displayName: "0 (Tx only)" },
                { name: 1, displayName: "1" },
                { name: 2, displayName: "2 (Multi-mic)" },
            ],
        },
        {
            name: "Sampling_Freq",
            displayName: "Sampling Frequency",
            description: "Audio sampling frequency in kHz",
            longDescription: `
Select the audio sampling frequency:
- **8 kHz**: Voice/telephony applications
- **16 kHz**: Wideband voice, basic audio
- **32 kHz**: Digital radio, some audio applications
- **48 kHz**: Professional audio, CD quality
- **96 kHz**: High-resolution audio

Note: Higher sampling frequencies require more PRU processing power and memory bandwidth.
            `,
            default: "48",
            options: [
                { name: "8", displayName: "8 kHz (Voice)" },
                { name: "16", displayName: "16 kHz (Wideband Voice)" },
                { name: "32", displayName: "32 kHz (Digital Radio)" },
                { name: "48", displayName: "48 kHz (CD Quality)" },
                { name: "96", displayName: "96 kHz (High-Res Audio)" },
            ],
        },
        {
            name: "Bits_Per_Slot",
            displayName: "Bits Per Slot",
            description: "Number of bits per I2S time slot",
            longDescription: `
Select the bit depth per I2S time slot:
- **16 bits**: Basic audio quality, lower bandwidth
- **24 bits**: Professional audio quality
- **32 bits**: Maximum audio quality, highest bandwidth

Note: Actual audio resolution depends on the codec/ADC/DAC capabilities.
The I2S interface uses this value for framing and timing.
            `,
            default: "32",
            options: [
                { name: "16", displayName: "16 bits" },
                { name: "24", displayName: "24 bits" },
                { name: "32", displayName: "32 bits" },
            ],
        },
        {
            name: "INTC_Config",
            displayName: "INTC Configuration",
            description: "PRU-ICSS interrupt controller event configuration",
            config: [
                {
                    name: "Tx_INTC_Event",
                    displayName: "Tx System Event",
                    description: "INTC system event number for Tx complete interrupt",
                    default: 18,
                },
                {
                    name: "Rx_INTC_Event",
                    displayName: "Rx System Event",
                    description: "INTC system event number for Rx complete interrupt",
                    default: 19,
                },
                {
                    name: "Err_INTC_Event",
                    displayName: "Error System Event",
                    description: "INTC system event number for error interrupt (overflow/underflow)",
                    default: 20,
                },
            ],
        },
        {
            name: "Clock_Config",
            displayName: "Clock Configuration",
            description: "PRU core and peripheral clock settings",
            config: [
                {
                    name: "Core_Clk_Freq",
                    displayName: "PRU Core Clock (Hz)",
                    description: "PRU core clock frequency in Hz",
                    default: 200000000,  /* 200 MHz typical */
                },
            ],
        },
    ],
    validate: onValidate,
    moduleStatic: {
        modules: function(inst) {
            let modArray = [];

            /* Add PRUICSS dependency */
            modArray.push({
                name: "pruicss",
                moduleName: "/drivers/pruicss/pruicss",
            });

            return modArray;
        },
    },
};

exports = pru_i2s_module;
