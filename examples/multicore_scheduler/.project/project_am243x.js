let path = require('path');

let device = "am243x";

let project_name = "multicore_scheduler";

const files_pru0 = {
    common: [
        "main.c",
        "pwm_toggle.asm",
        "AM243x_PRU0.cmd",
        "AM243_AM64_PRU_pinmux.h"
    ],
};
const files_pru1 = {
    common: [
        "main.c",
        "AM243x_PRU1.cmd",
        "pwm_toggle.asm"
    ],
};
const files_rtu0 = {
    common: [
        "main.c",
        "AM243x_RTU0.cmd",
        "pwm_toggle.asm"
    ],
};
const files_rtu1 = {
    common: [
        "main.c",
        "AM243x_RTU1.cmd",
        "pwm_toggle.asm"
    ],
};
const files_txpru0 = {
    common: [
        "main.c",
        "AM243x_TX_PRU0.cmd",
        "pwm_toggle.asm"
    ],
};
const files_txpru1 = {
    common: [
        "main.c",
        "AM243x_TX_PRU1.cmd",
        "pwm_toggle.asm"
    ],
};
/* Relative to where the makefile will be generated
 * Typically at <example_folder>/<BOARD>/<core_os_combo>/<compiler>
 */
const filedirs = {
    common: [
        "..",       /* core_os_combo base */
        "../../..", /* Example base */
        "."
    ],
};

const includes = {
    common: [
        "${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/common",
    ],
};

const lnkfiles_pru0 = {
    common: [
        "AM243x_PRU0.cmd",
    ]
};
const lnkfiles_pru1 = {
    common: [
        "AM243x_PRU1.cmd",
    ]
};
const lnkfiles_rtu0 = {
    common: [
        "AM243x_RTU0.cmd",
    ]
};
const lnkfiles_rtu1 = {
    common: [
        "AM243x_RTU1.cmd",
    ]
};
const lnkfiles_txpru0 = {
    common: [
        "AM243x_TX_PRU0.cmd",
    ]
};
const lnkfiles_txpru1 = {
    common: [
        "AM243x_TX_PRU1.cmd",
    ]
};

const lflags = {
    common: [
        "--entry_point=main",
        "--diag_suppress=10063-D", /* Added to suppress entry_point related warning */
    ],
};

const readmeDoxygenPageTag = "EXAMPLES_Multicore_Scheduler_AM243";

const templates_pru =
[
    
];


const buildOptionCombos = [

    { device: device, cpu: "icss_g0_pru0", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},

    { device: device, cpu: "icss_g0_pru1", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},

    { device: device, cpu: "icss_g0_rtu_pru0", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},

    { device: device, cpu: "icss_g0_rtu_pru1", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},

    { device: device, cpu: "icss_g0_tx_pru0", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},

    { device: device, cpu: "icss_g0_tx_pru1", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},
];

function getmakefilePruPostBuildSteps(cpu, board)
{
    let core = "PRU0"

    switch(cpu)
    {
        case "icss_g0_tx_pru1":
            core = "TXPRU1"
            break;
        case "icss_g0_tx_pru0":
            core = "TXPRU0"
            break;
        case "icss_g0_rtu_pru1":
            core = "RTUPRU1"
            break;
        case "icss_g0_rtu_pru0":
            core = "RTUPRU0"
            break;
        case "icss_g0_pru1":
            core = "PRU1"
            break;
        case "icss_g0_pru0":
            core = "PRU0"
    }
    return  [
        
    ]; 
}

function getccsPruPostBuildSteps(cpu, board)
{
    let core = "PRU0"

    switch(cpu)
    {
        case "icss_g0_tx_pru1":
            core = "TXPRU1"
            break;
        case "icss_g0_tx_pru0":
            core = "TXPRU0"
            break;
        case "icss_g0_rtu_pru1":
            core = "RTUPRU1"
            break;
        case "icss_g0_rtu_pru0":
            core = "RTUPRU0"
            break;
        case "icss_g0_pru1":
            core = "PRU1"
            break;
        case "icss_g0_pru0":
            core = "PRU0"
    }
    return  [

    ]; 
}

function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "executable";
    property.makefile = "pru";
    property.name = project_name;
    property.isInternal = false;
    property.description = "A Multicore time based Task Scheduler that cyclicly schedules tasks on six PRU cores deterministically"
    property.buildOptionCombos = buildOptionCombos;
    property.skipUpdatingTirex = true;

    return property;
}

function getComponentBuildProperty(buildOption) {
    let build_property = {};

    if(buildOption.cpu=="icss_g0_pru0"){
        build_property.files = files_pru0;
    }
    else if(buildOption.cpu=="icss_g0_pru1"){
        build_property.files = files_pru1;
    }
    else if(buildOption.cpu=="icss_g0_rtu_pru0"){
        build_property.files = files_rtu0;
    }
    else if(buildOption.cpu=="icss_g0_rtu_pru1"){
        build_property.files = files_rtu1;
    }
    else if(buildOption.cpu=="icss_g0_tx_pru0"){
        build_property.files = files_txpru0;
    }
    else{
        build_property.files = files_txpru1;
    }
    build_property.filedirs = filedirs;
    if(buildOption.cpu=="icss_g0_pru0"){
        build_property.lnkfiles = lnkfiles_pru0;
    }
    else if(buildOption.cpu=="icss_g0_pru1"){
        build_property.lnkfiles = lnkfiles_pru1;
    }
    else if(buildOption.cpu=="icss_g0_rtu_pru0"){
        build_property.lnkfiles = lnkfiles_rtu0;
    }
    else if(buildOption.cpu=="icss_g0_rtu_pru1"){
        build_property.lnkfiles = lnkfiles_rtu1;
    }
    else if(buildOption.cpu=="icss_g0_tx_pru0"){
        build_property.lnkfiles = lnkfiles_txpru0;
    }
    else{
        build_property.lnkfiles = lnkfiles_txpru1;
    }
    build_property.includes = includes;
    build_property.lflags = lflags;
    build_property.templates = templates_pru;
    build_property.projecspecFileAction = "copy";
    build_property.skipMakefileCcsBootimageGen = true;
    build_property.ccsPruPostBuildSteps = getccsPruPostBuildSteps(buildOption.cpu, buildOption.board);
    build_property.makefilePruPostBuildSteps = getmakefilePruPostBuildSteps(buildOption.cpu, buildOption.board);

    return build_property;
}

module.exports = {
    getComponentProperty,
    getComponentBuildProperty,
};
