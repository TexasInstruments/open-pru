let path = require('path');

let device = "am243x";

const files = {
    common: [
        "main.c",
        "spi_pru_demo.c"
    ],
};

/* Relative to where the makefile will be generated
 * Typically at <example_folder>/<BOARD>/<core_os_combo>/<compiler>
 */
const filedirs = {
    common: [
        "..",       /* core_os_combo base */
        "../..",
        "../../..", /* Example base */
        "../../../firmware",
    ],
};

const libdirs_freertos = {
    common: [
        "${MCU_PLUS_SDK_PATH}/source/kernel/freertos/lib",
        "${MCU_PLUS_SDK_PATH}/source/drivers/lib",
        "${MCU_PLUS_SDK_PATH}/source/board/lib",
        "${MCU_PLUS_SDK_PATH}/source/pru_io/lib",
        "${CG_TOOL_ROOT}/lib",
    ],
};

const libdirs_nortos = {
    common: [
        "${MCU_PLUS_SDK_PATH}/source/kernel/nortos/lib",
        "${MCU_PLUS_SDK_PATH}/source/drivers/lib",
        "${MCU_PLUS_SDK_PATH}/source/board/lib",
        "${MCU_PLUS_SDK_PATH}/source/pru_io/lib",
        "${CG_TOOL_ROOT}/lib",
    ],
};

const includes_freertos_r5f_am243x_lp = {
    common: [
        "${MCU_PLUS_SDK_PATH}/source/kernel/freertos/FreeRTOS-Kernel/include",
        "${MCU_PLUS_SDK_PATH}/source/kernel/freertos/portable/TI_ARM_CLANG/ARM_CR5F",
        "${MCU_PLUS_SDK_PATH}/source/kernel/freertos/config/am243x/r5f",
        "${MCU_PLUS_SDK_PATH}/source/pru_io/driver",
        "${OPEN_PRU_PATH}/examples/spi_loopback/spi_loopback_app/firmware",
        "${OPEN_PRU_PATH}/examples/spi_loopback/spi_loopback_app/am243x-lp",
        "${CG_TOOL_ROOT}/include/c",
    ],
};

const includes_nortos_r5f_am243x_lp = {
    common: [
        "${MCU_PLUS_SDK_PATH}/source/kernel/nortos/dpl/common",
        "${MCU_PLUS_SDK_PATH}/source/kernel/nortos/dpl/r5f",
        "${MCU_PLUS_SDK_PATH}/source/pru_io/driver",
        "${OPEN_PRU_PATH}/examples/spi_loopback/spi_loopback_app/firmware",
        "${OPEN_PRU_PATH}/examples/spi_loopback/spi_loopback_app/am243x-lp",
        "${CG_TOOL_ROOT}/include/c",
    ],
};

const libs_freertos_r5f = {
    common: [
        "freertos.am243x.r5f.ti-arm-clang.${ConfigName}.lib",
        "drivers.am243x.r5f.ti-arm-clang.${ConfigName}.lib",
        "board.am243x.r5f.ti-arm-clang.${ConfigName}.lib",
        "libc.a",
        "libsysbm.a",
    ],
};

const libs_nortos_r5f = {
    common: [
        "nortos.am243x.r5f.ti-arm-clang.${ConfigName}.lib",
        "drivers.am243x.r5f.ti-arm-clang.${ConfigName}.lib",
        "board.am243x.r5f.ti-arm-clang.${ConfigName}.lib",
        "libc.a",
        "libsysbm.a",
    ],
};

const lnkfiles = {
    common: [
        "linker.cmd",
    ]
};

const syscfgfile = "../example.syscfg"

const templates_freertos_r5f =
[
    {
        input: ".project/templates/am243x/freertos/main_freertos.c.xdt",
        output: "../main.c",
        options: {
            entryFunction: "SPI_pru_demo_main",
        },
    }
];

const templates_nortos_r5f =
[
    {
        input: ".project/templates/am243x/nortos/main_nortos.c.xdt",
        output: "../main.c",
        options: {
            entryFunction: "SPI_pru_demo_main",
        },
    }
];

const buildOptionCombos = [
    { device: device, cpu: "r5fss0-0", cgt: "ti-arm-clang", board: "am243x-lp", os: "freertos"},
    { device: device, cpu: "r5fss0-0", cgt: "ti-arm-clang", board: "am243x-lp", os: "nortos"},
];


function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "executable";
    property.name = "spi_loopback";
    property.isInternal = false;
    property.description = "R5F PRU IO Project for SPI loop back example"
    property.buildOptionCombos = buildOptionCombos;
    property.skipUpdatingTirex = true;

    return property;
}

function getComponentBuildProperty(buildOption) {
    let build_property = {};

    build_property.files = files;
    build_property.filedirs = filedirs;
    if(buildOption.os=="freertos"){
    build_property.includes = includes_freertos_r5f_am243x_lp;
    build_property.libdirs = libdirs_freertos;
    build_property.libs = libs_freertos_r5f;
    build_property.templates = templates_freertos_r5f;
    }else{
    build_property.includes = includes_nortos_r5f_am243x_lp;
    build_property.libdirs = libdirs_nortos;
    build_property.libs = libs_nortos_r5f;
    build_property.templates = templates_nortos_r5f;
    }
    build_property.lnkfiles = lnkfiles;
    build_property.syscfgfile = syscfgfile;
    
    return build_property;
}

module.exports = {
    getComponentProperty,
    getComponentBuildProperty,
};
