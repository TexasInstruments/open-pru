
let common = system.getScript("/common");
let I2S_module_name = "/pru_i2s/pru_i2s";
let device = common.getDeviceName();
let is_am261x_soc = (device === "am261x-lp") ? true : false;
let I2S_module = {
    displayName: "PRU-I2S Emulation",
    templates: {
        "/common/open_pru_config.c.xdt": {
            driver_config: "/pru_i2s/i2s/pru_i2s_templates.h.xdt",
            moduleName: I2S_module_name,
        },
    },
    defaultInstanceName: "CONFIG_I2S",
    config: [
        {
            name: "instance",
            displayName: "Instance",
            default: (is_am261x_soc) ? "ICSSM1"  : "ICSSG0",
            options: (is_am261x_soc) ?
                        [
                            {
                                name: "ICSSM0",
                            },
                            {
                                name: "ICSSM1",
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
            name: "coreClk",
            displayName: "PRU-ICSS Core Clk (Hz)",
            default: 225*1000000,
        },
        {
            name: "PRU_Slice",
            displayName: "Select PRU Slice",
            description: "ICSSM PRU Slice",
            hidden :(is_am261x_soc) ? false : true,
            default: "PRU1",
            options: [
                {
                    name: "PRU0",
                },
                {
                    name: "PRU1",
                },
            ],
        },
        {
            name: "Mode",
            displayName: "Select Mode",
            description: "Mode selection",
            default: "I2S",
            options: [
                {
                    name: "I2S",
                },
                {
                    name: "TDM",
                },
            ],
        },
    ],
    
    sharedModuleInstances: sharedModuleInstances,
};

function sharedModuleInstances(instance) {
    let modInstances = new Array();
    let requiredArgs =  (is_am261x_soc) ? {instance:`${instance.instance}`} : {instance: instance.instance ,coreClk:instance.coreClk};

    modInstances.push({
        name: "pru",
        displayName: "PRU ICSS Configuration",
        moduleName: '/drivers/pruicss/pruicss',
        requiredArgs
    });
    return (modInstances);
}

exports = I2S_module;
