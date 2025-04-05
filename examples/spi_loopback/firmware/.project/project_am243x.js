let path = require('path');

let device = "am243x";

const files_pru0 = {
    common: [
        "main.asm",
        "linker.cmd",
        "spi_master_macros.inc"
    ],
};

const files_pru1 = {
    common: [
        "main.asm",
        "linker.cmd",
        "spi_slave_macros.inc"
    ],
};

/* Relative to where the makefile will be generated
 * Typically at <example_folder>/<BOARD>/<core_os_combo>/<compiler>
 */
const filedirs = {
    common: [
        "..",       /* core_os_combo base */
        "../..",    /* inc files */
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
        "linker.cmd",
    ]
};

const lflags = {
    common: [
        "--entry_point=main",
        "--diag_suppress=10063-D", /* Added to suppress entry_point related warning */
    ],
};

const buildOptionCombos = [
    { device: device, cpu: "icss_g0_pru0", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},
    { device: device, cpu: "icss_g0_pru1", cgt: "ti-pru-cgt", board: "am243x-lp", os: "fw"},
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

    return [
        " $(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix="+ core + "Firmware  -o "+ core.toLocaleLowerCase() + "_load_bin.h " + "spi_loopback_" + board + "_" + cpu + "_fw_ti-pru-cgt.out; $(SED) -i '0r ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h' "+ core.toLocaleLowerCase() + "_load_bin.h ; $(MOVE) "+ core.toLocaleLowerCase() + "_load_bin.h " + "${OPEN_PRU_PATH}/examples/spi_loopback/spi_loopback_app/firmware/"+core.toLocaleLowerCase() + "_load_bin.h "
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

    return [
        " $(CG_TOOL_ROOT)/bin/hexpru --diag_wrap=off --array --array:name_prefix="+ core + "Firmware  -o "+ core.toLocaleLowerCase() + "_load_bin.h " + "spi_loopback_" + board + "_" + cpu + "_fw_ti-pru-cgt.out; if ${CCS_HOST_OS} == win32 $(CCS_INSTALL_DIR)/utils/cygwin/sed -i '0r ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h' "+ core.toLocaleLowerCase() + "_load_bin.h ; if ${CCS_HOST_OS} == linux sed -i '0r ${MCU_PLUS_SDK_PATH}/source/pru_io/firmware/pru_load_bin_copyright.h' "+ core.toLocaleLowerCase() + "_load_bin.h ;" + "if ${CCS_HOST_OS} == win32 move "+ core.toLocaleLowerCase() + "_load_bin.h " + "${OPEN_PRU_PATH}/examples/spi_loopback/spi_loopback_app/firmware/"+core.toLocaleLowerCase() + "_load_bin.h; if ${CCS_HOST_OS} == linux mv "+ core.toLocaleLowerCase() + "_load_bin.h " + "${OPEN_PRU_PATH}/examples/spi_loopback/spi_loopback_app/firmware/"+core.toLocaleLowerCase() + "_load_bin.h "
    ];
}

function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "executable";
    property.makefile = "pru";
    property.name = "spi_loopback";
    property.isInternal = false;
    property.description = "PRU Projects for spi loopback demo. PRU0 and PRU1 cores of ICSSG0 emulate spi master and device respectively"
    property.buildOptionCombos = buildOptionCombos;
    property.pru_main_file = "main";
    property.pru_linker_file = "linker";
    property.skipUpdatingTirex = true;

    return property;
}

function getComponentBuildProperty(buildOption) {
    let build_property = {};
    if(buildOption.cpu=="icss_g0_pru0"){
        build_property.files = files_pru0;
    }
    else{
        build_property.files = files_pru1;
    }
    build_property.filedirs = filedirs;
    build_property.lnkfiles = lnkfiles;
    build_property.includes = includes;
    build_property.lflags = lflags;
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
