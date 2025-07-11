let path = require('path');

let device = "am261x";

const files = {
    common: [
        "pru_i2s_drv.c",
        
    ],
};


const filedirs = {
    common: [
        "${OPEN_PRU_PATH}/source/pru_i2s/driver",
    ],
};

const includes = {
    common: [
        "${OPEN_PRU_PATH}/source/pru_i2s/include","${OPEN_PRU_PATH}/source/pru_i2s/include",
    ],
};

const buildOptionCombos = [
    { device: device, cpu: "r5f", cgt: "ti-arm-clang"},
];

function getComponentProperty() {
    let property = {};

    property.dirPath = path.resolve(__dirname, "..");
    property.type = "library";
    property.name = "pru_i2s";
    property.isInternal = false;
    property.buildOptionCombos = buildOptionCombos;

    return property;
}

function getComponentBuildProperty(buildOption) {
    let build_property = {};

    build_property.files = files;
    build_property.filedirs = filedirs;
    build_property.includes = includes;

    return build_property;
}

module.exports = {
    getComponentProperty,
    getComponentBuildProperty,
};
