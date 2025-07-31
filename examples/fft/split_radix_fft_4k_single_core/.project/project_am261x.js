let path = require('path');

let device = "am261x";

let project_name = "split_radix_fft_4k_single_core";

const files = {
    common: [
        "AM261x_PRU0.cmd",
        "butterfly_length_2.asm",
        "fft_windowed_4k.asm",
        "window_function_4k.asm",
        "lut_load.asm",
        "fft_macros.inc",
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
        "AM261x_PRU0.cmd",
    ]
};

const lflags = {
    common: [
        "--entry_point=main",
        "--diag_suppress=10063-D", /* Added to suppress entry_point related warning */
    ],
};

const readmeDoxygenPageTag = "EXAMPLES_FFT_AM261";
const templates_pru =
[

];

const buildOptionCombos = [
    { device: device, cpu: "icss_m0_pru0", cgt: "ti-pru-cgt", board: "am261x-lp", os: "fw"},
];

function getmakefilePruPostBuildSteps(cpu, board)
{
    let core = "PRU0"

    switch(cpu)
    {
        case "icss_m0_pru1":
            core = "PRU1"
            break;
        case "icss_m0_pru0":
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
        case "icss_m0_pru1":
            core = "PRU1"
            break;
        case "icss_m0_pru0":
            core = "PRU0"
    }

    return [
        
    ]; 
}

function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "executable";
    property.makefile = "pru";
    property.name = project_name;
    property.isInternal = false;
    property.description = "Real valued 4K FFT using split-radix algorithm"
    property.buildOptionCombos = buildOptionCombos;
    property.isSkipTopLevelBuild = true;
    property.skipUpdatingTirex = true;

    return property;
}

function getComponentBuildProperty(buildOption) {
    let build_property = {};

    build_property.files = files;
    build_property.filedirs = filedirs;
    build_property.lnkfiles = lnkfiles;
    build_property.lflags = lflags;
    build_property.includes = includes;
    build_property.templates = templates_pru;
    build_property.readmeDoxygenPageTag = readmeDoxygenPageTag;
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
