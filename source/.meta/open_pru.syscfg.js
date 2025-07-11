
let common = system.getScript("/common");
let soc = system.getScript(`/soc/pru_i2s_${common.getSocName()}`);

function getTemplates()
{
    let templates = [];
    
        templates.push(
            {
                name: "/common/open_pru_config.c.xdt",
                outputPath: "ti_open_pru_config.h",
                alwaysRun: true,
            },
        )
    
    return templates;
}
exports = {
    displayName: "OPEN PRU",
    templates: getTemplates(),
    topModules: soc.getTopModules(),
};
