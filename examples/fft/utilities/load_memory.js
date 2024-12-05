
/*
Description:
    Load the sample inputs and LUTs to the memory

Usage:

    1. Replace the variable DatPath with your path to the data folder

    2. Launch AM64x target connection in CCS and connect to PRU0

    3. Run script using 'loadJSFile' command.

*/

var DatPath = "<add_your_data_folder_path_here>"; //data location
var Ipfile = "sample_input_4k.dat"; // name of the input dat file 
var IpPath = DatPath + "\\" + Ipfile;
var WinPath = DatPath + "\\hanning_window_q32_4k_halved.dat";
var TwPath = DatPath + "\\twiddle_factor_lut_4k.dat";
var BrPath = DatPath + "\\bit_reversed_values_9bit.dat";

var IP_RAM_ADDR_BASE        = 0x00010000;   // base address of input values 
var WC_RAM_ADDR_BASE        = 0x00018000;   // base address of window coefficient LUT 
var TW_FACTOR_START_ADDRESS = 0x0001A000;   // base address of twiddle factor LUT
var BRV_RAM_ADDR_BASE        = 0x0001E000;  // base address of bit-reversed LUT 

function loadmemory()
{
    loadRaw(IP_RAM_ADDR_BASE,2,IpPath,32,false)

    loadRaw(WC_RAM_ADDR_BASE,2,WinPath,32,false)
   
    loadRaw(TW_FACTOR_START_ADDRESS,2,TwPath,32,false)
   
    loadRaw(BRV_RAM_ADDR_BASE,2,BrPath,32,false)
   
}

loadmemory()


