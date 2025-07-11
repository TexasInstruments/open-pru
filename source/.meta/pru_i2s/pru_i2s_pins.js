
let common = system.getScript("/common");
let pinmux = system.getScript("/drivers/pinmux/pinmux");


function getInterfaceName(inst)
{
    return `PRU_${inst.instance}_PRU`;
}

function getPeripheralPinNames(inst)
{
    let pinList = [];
    let pinNameList = [];

    pinList = getInterfacePinList(inst);

    for(let pin of pinList)
    {
        pinNameList.push( pin.pinName );
    }

    return pinNameList;
}


exports = {

    getInterfaceName,
    getPeripheralPinNames,
};
