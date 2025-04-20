let path = require('path');

let device = "am243x";

const files = {
    common: [
        "main.asm",
        "linker.cmd"
    ],
};

/* Relative to where the makefile will be generated
 * Typically at <example_folder>/<BOARD>/<core_os_combo>/<compiler>
 */
const filedirs = {
    common: [
        "..",       /* core_os_combo base */
        "../../../", /* Example base */
        ".",
        "../am243x-evm"
    ],
};

const includes = {
    common: [
        "${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/common",
        "${OPEN_PRU_PATH}/examples/pru_emif/firmware/include",
        "${OPEN_PRU_PATH}/examples/pru_emif/firmware/pru1",
    ],
};

const lnkfiles = {
    common: [
        "linker.cmd",
    ]
};
const templates_pru =
[
    {
        input: ".project/templates/am243x/common/pru/linker_pru0.cmd.xdt",
        output: "linker.cmd",
    }
];

const buildOptionCombos = [
    { device: device, cpu: "icss_g0_pru1", cgt: "ti-pru-cgt", board: "am243x-evm", os: "fw"},
];

const hexBuildOptions = [
    "--diag_wrap=off",
    "--array",
    "--array:name_prefix=PRUFirmware",
    "-o=firmware_binary.h",
];
const cflags = {
    common: [
        "-v4"
    ],
};

const lflags = {
    common: [
        "--entry_point=main",
        "--diag_suppress=10063-D", /* Added to suppress entry_point related warning */
    ],
};

function getmakefilePruPostBuildSteps(cpu, board)
{
    let core = "PRU0"

    switch(cpu)
    {
        case "icss_g0_pru1":
            core = "PRU1"
            break;
        case "icss_g0_pru0":
            core = "PRU0"
    }

    return  [
        "$(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix=prue_emif_data_"+core.toLocaleLowerCase()+" -o pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h pru_emif_data_"+core.toLocaleLowerCase()+"_fw_" + board + "_" + cpu + "_fw_ti-pru-cgt.out; $(SED) -i '0r ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h' pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h ; $(MOVE) pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h ${OPEN_PRU_PATH}/examples/pru_emif/pru_emif_app/Firmware/pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h"
    ];
}

function getccsPruPostBuildSteps(cpu, board)
{
    let core = "PRU0"

    switch(cpu)
    {
        case "icss_g0_pru1":
            core = "PRU1"
            break;
        case "icss_g0_pru0":
            core = "PRU0"
    }

    return  [
        "$(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix=pru_emif_data_"+core.toLocaleLowerCase()+" -o pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h pru_emif_data_"+core.toLocaleLowerCase()+"_fw_" + board + "_" + cpu + "_fw_ti-pru-cgt.out; if ${CCS_HOST_OS} == win32 $(CCS_INSTALL_DIR)/utils/cygwin/sed -i '0r ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h' pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h ; if ${CCS_HOST_OS} == linux sed -i '0r ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h' pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h; if ${CCS_HOST_OS} == win32 $(CCS_INSTALL_DIR)/utils/cygwin/mv pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h ${OPEN_PRU_PATH}/examples/pru_emif/pru_emif_app/Firmware/pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h; if ${CCS_HOST_OS} == linux mv pru_data_"+core.toLocaleLowerCase()+"_bin.h ${OPEN_PRU_PATH}/examples/pru_emif/pru_emif_app/Firmware/pru_emif_data_"+core.toLocaleLowerCase()+"_bin.h"
    ];
}

function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "executable";
    property.makefile = "pru";
    property.name = "pru_emif_data_pru1_fw";
    property.isInternal = false;
    property.description = "PRU EMIF Data Handle Firmware"
    property.buildOptionCombos = buildOptionCombos;
    property.pru_main_file = "main";
    property.pru_linker_file = "linker";
    property.isSkipTopLevelBuild = true;
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
    build_property.projecspecFileAction = "copy";
    build_property.skipMakefileCcsBootimageGen = true;
    build_property.templates = templates_pru;
    build_property.ccsPruPostBuildSteps = getccsPruPostBuildSteps(buildOption.cpu, buildOption.board);
    build_property.makefilePruPostBuildSteps = getmakefilePruPostBuildSteps(buildOption.cpu, buildOption.board);


    return build_property;
}

module.exports = {
    getComponentProperty,
    getComponentBuildProperty,
};
