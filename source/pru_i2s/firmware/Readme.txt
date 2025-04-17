Steps for generating I2S binary header files is given below.
1. In the setupenv.bat file, provide correct paths for CCS(CCS_INSTALL_DIR), XDC tools version and PRU code generation tool(PRU_CGT_BS).
2. Open the command prompt and run the setupenv.bat file.
3. In the same window, run gmake. 
	-PRU0 Firmware build:
	 a.To build : gmake PRU_CORE=0 I2S_TX=1 NUMBER_OF_TX_3=1 I2S_RX=0 I2S_RX_DETECT_OVERFLOW=0 I2S_TX_DETECT_UNDERFLOW=1 (For 3 I2S Tx Only)
	              gmake PRU_CORE=0 I2S_TX=1 NUMBER_OF_TX_2=1 I2S_RX=0 I2S_RX_DETECT_OVERFLOW=0 I2S_TX_DETECT_UNDERFLOW=1 (For 2 I2S Tx Only) 
	 b.Generated file is prui2s0_array.h
	 c.To clean : gmake clean PRU_CORE=0	 
	-PRU1 Firmware build:
	 a.To build : gmake PRU_CORE=1 I2S_TX=0 I2S_RX=1 I2S_RX_DETECT_OVERFLOW=1 I2S_TX_DETECT_UNDERFLOW=0
	 b.Generated file is prui2s1_array.h
	 c.To clean : gmake clean PRU_CORE=1