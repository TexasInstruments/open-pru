
/*
Description:
    Load the sample inputs and LUTs to the memory

Usage:

    1. Set Device to "AM261" or "AM243"

    2. Replace the variable DatPath with your path to the data folder

    3. Launch device specific target connection in CCS and connect to an R5SS core

    4. Run script using 'loadJSFile' command.

*/

var Device = "AM243";    //change to "AM261" for the other supported device

var DatPath = "<add_your_data_folder_path_her>"; //data location
var Ipfile = "Coherent_samples_50Hz_offseted.dat"; // name of the input dat file 
var IpPath = DatPath + "\\" + Ipfile;
var WinPath = DatPath + "\\hanning_window_q32_4k_halved.dat";
var TwPath = DatPath + "\\twiddle_factor_lut_4k.dat";
var BrPath = DatPath + "\\bit_reversed_values_9bit.dat";
if (Device == "AM261")
{
    var IP_RAM_ADDR_BASE        = 0x48010000;   // ICSSM SRAM, base address of input values 
    var WC_RAM_ADDR_BASE        = 0x78000000;   // TCMA, base address of window coefficient LUT 
    var TW_FACTOR_START_ADDRESS = 0x48000000;   // ICSSM DRAM, base address of twiddle factor LUT
    var BRV_RAM_ADDR_BASE       = 0x78002000;   // base address of bit-reversed LUT 
}
else if (Device == "AM243") 
{
    var IP_RAM_ADDR_BASE         = 0x30010000;   // base address of input values 
    var WC_RAM_ADDR_BASE         = 0x30018000;   // base address of window coefficient LUT 
    var TW_FACTOR_START_ADDRESS  = 0x3001A000;   // base address of twiddle factor LUT
    var BRV_RAM_ADDR_BASE        = 0x3001E000;   // base address of bit-reversed LUT 
}
else //
{
    console.error("Unrecognized device type:", Device);
    throw new Error("Execution stopped due to unknown device");
}


function loadmemory()
{
    loadRaw(IP_RAM_ADDR_BASE,0,IpPath,32,false);

    loadRaw(WC_RAM_ADDR_BASE,0,WinPath,32,false);
   
    loadRaw(TW_FACTOR_START_ADDRESS,0,TwPath,32,false);
   
    loadRaw(BRV_RAM_ADDR_BASE,0,BrPath,32,false);
   
}

loadmemory();


