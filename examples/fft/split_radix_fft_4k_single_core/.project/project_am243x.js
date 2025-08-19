let path = require('path');

let device = "am243x";

const files = {
    common: [
    "AM243x_PRU0.cmd",
    "butterfly_length_2.asm",
    "fft_windowed_4k.asm",
    "window_function_4k.asm",
    "lut_load.asm",
    "main.asm"
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

const lnkfiles = {
    common: [
        "AM243x_PRU0.cmd",
    ]
};

const lflags = {
    common: [
        "--entry_point=main",
        "--diag_suppress=10063-D", /* Added to suppress entry_point related warning */
    ],
};


const templates_pru =
[
    
];

const buildOptionCombos = [
    { device: device, cpu: "icss_g0_pru0", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"}
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
        "$(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix="+ core + "Firmware  -o "+ core.toLocaleLowerCase() + "_load_bin.h " + "split_radix_fft_4k_single_core" + "_" + board + "_" + cpu + "_fw_ti-pru-cgt.out;"+ 
        "$(CAT)  ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h "+ core.toLocaleLowerCase() + "_load_bin.h > ${OPEN_PRU_PATH}/examples/split_radix_fft_4k_single_core/firmware/"+ board + "/" +core.toLocaleLowerCase() + "_load_bin.h ;"+ 
        "$(RM) "+ core.toLocaleLowerCase() + "_load_bin.h;"
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
        "$(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix="+ core + "Firmware  -o "+ core.toLocaleLowerCase() + "_load_bin.h " + "split_radix_fft_4k_single_core" + "_" + board + "_" + cpu + "_fw_ti-pru-cgt.out;"+ 
        "if ${CCS_HOST_OS} == linux cat ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h "+ core.toLocaleLowerCase() + "_load_bin.h > ${OPEN_PRU_PATH}/examples/split_radix_fft_4k_single_core/firmware/"+ board + "/" +core.toLocaleLowerCase() + "_load_bin.h ;"+ 
        "if ${CCS_HOST_OS} == linux rm "+ core.toLocaleLowerCase() + "_load_bin.h;"+
        "if ${CCS_HOST_OS} == win32  $(CCS_INSTALL_DIR)/utils/cygwin/cat ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h "+ core.toLocaleLowerCase() + "_load_bin.h > ${OPEN_PRU_PATH}/examples/split_radix_fft_4k_single_core/firmware/"+ board + "/" +core.toLocaleLowerCase() + "_load_bin.h ;"+ 
        "if ${CCS_HOST_OS} == win32  $(CCS_INSTALL_DIR)/utils/cygwin/rm "+ core.toLocaleLowerCase() + "_load_bin.h;"
    ]; 
}

function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "executable";
    property.makefile = "pru";
    property.name = "split_radix_fft_4k_single_core";
    property.isInternal = false;
    property.description = "Real valued 4K FFT using split-radix algorithm"
    property.buildOptionCombos = buildOptionCombos;
    property.skipUpdatingTirex = true;

    return property;
}

function getComponentBuildProperty(buildOption) {
    let build_property = {};

    build_property.files = files;
    build_property.filedirs = filedirs;
    build_property.lnkfiles = lnkfiles;
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
