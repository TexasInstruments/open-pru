let path = require('path');

let device = "am261x";

const files = {
    common: [
        "pru_i2s_main.asm",
        "fw_regs.asm",
        "pru_i2s_master_icss.cmd",
    ],
};

/* Relative to where the makefile will be generated
 * Typically at <example_folder>/<BOARD>/<core_os_combo>/<compiler>
 */
const filedirs = {
    common: [
        "..",       /* core_os_combo base */
        "../../..", /* Example base */
        "../../../..",
    ],
};

const includes = {
    common: [
        "${OPEN_PRU_PATH}/source/pru_i2s/firmware/TDM4",
    ],
};

const defines = {
    common: [
        "PRU0",
        "I2S_TX",
        "NUMBER_OF_TX_3",
        "I2S_TX_DETECT_UNDERFLOW",
    ],
};

const lflags = {
    common: [
        "--disable_auto_rts",
        "--diag_suppress=10063-D", /* Added to suppress entry_point related warning */
        "--entry_point=main",
    ],
};

function getmakefilePruPostBuildSteps(cpu, board)
{
    return  [
        "$(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix=pru_prupru_i2s0_image_0 -o pru_i2s_tdm4_pru0_array.h  pru_i2s_tdm4_pru0_array_" + board + "_" + cpu + "_fw_ti-pru-cgt.out;"+ 
        "$(CAT) ${OPEN_PRU_PATH}/mcu_plus_sdk/source/pru_io/firmware/pru_load_bin_copyright.h pru_i2s_tdm4_pru0_array.h > ${OPEN_PRU_PATH}/source/pru_i2s/firmware/TDM4/pru_i2s_tdm4_pru0_array.h;"+ 
        "$(RM) pru_i2s_tdm4_pru0_array.h;"
    ];
}

function getccsPruPostBuildSteps(cpu, board)
{
    return  [
        "$(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix=pru_prupru_i2s0_image_0 -o pru_i2s_tdm4_pru0_array.h  pru_i2s_tdm4_pru0_array_" + board + "_" + cpu + "_fw_ti-pru-cgt.out;"+ 
        "if ${CCS_HOST_OS} == linux cat ${OPEN_PRU_PATH}/mcu_plus_sdk/source/pru_io/firmware/pru_load_bin_copyright.h pru_i2s_tdm4_pru0_array.h > ${OPEN_PRU_PATH}/source/pru_i2s/firmware/TDM4/pru_i2s_tdm4_pru0_array.h;"+ 
        "if ${CCS_HOST_OS} == linux rm pru_i2s_tdm4_pru0_array.h;"+
        "if ${CCS_HOST_OS} == win32  $(CCS_INSTALL_DIR)/utils/cygwin/cat ${OPEN_PRU_PATH}/mcu_plus_sdk/source/pru_io/firmware/pru_load_bin_copyright.h pru_i2s_tdm4_pru0_array.h > ${OPEN_PRU_PATH}/source/pru_i2s/firmware/TDM4/pru_i2s_tdm4_pru0_array.h;"+ 
        "if ${CCS_HOST_OS} == win32  $(CCS_INSTALL_DIR)/utils/cygwin/rm pru_i2s_tdm4_pru0_array.h;"
    ];
}


const readmeDoxygenPageTag = "pru_i2s_DESIGN";

const buildOptionCombos = [
    { device: device, cpu: "icssm0-pru0", cgt: "ti-pru-cgt", board: "am261x-lp", os: "fw"},
];

function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "executable";
    property.makefile = "pru";
    property.name = "pru_i2s_tdm4_pru0_array";
    property.description = "pru_i2s Firmware for PRU-ICSS running at 225 MHz";
    property.isInternal = false;
    property.buildOptionCombos = buildOptionCombos;
    property.pru_main_file = "pru_i2s_main";
    property.pru_linker_file = "pru_i2s_master_icss";
    property.isSkipTopLevelBuild = true;
    property.skipUpdatingTirex = true;

    return property;
}

function getComponentBuildProperty(buildOption) {
    let build_property = {};

    build_property.files = files;
    build_property.filedirs = filedirs;
    build_property.includes = includes;
    build_property.defines = defines;
    build_property.lflags = lflags;
    build_property.readmeDoxygenPageTag = readmeDoxygenPageTag;
    build_property.ccsPruPostBuildSteps = getccsPruPostBuildSteps(buildOption.cpu, buildOption.board);
    build_property.makefilePruPostBuildSteps = getmakefilePruPostBuildSteps(buildOption.cpu, buildOption.board);
    build_property.projecspecFileAction = "copy";
    build_property.skipMakefileCcsBootimageGen = true;

    return build_property;
}

module.exports = {
    getComponentProperty,
    getComponentBuildProperty,
};
