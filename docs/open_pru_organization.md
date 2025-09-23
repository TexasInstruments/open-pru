<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>
# OpenPRU Organization

</div>

[Repository file structure](#repository-file-structure)  
[Project structure](#project-structure)  

## Repository file structure

The repo is organized like this:

```
academy/
	getting started & training labs
examples/
	example PRU projects
source/
	linker.cmd files
	function macros
	register defines
	drivers
```

## Project structure

Projects are typically organized like this:

### PRU CODE

```
firmware/
	source code that is shared across multiple PRU cores, devices, or boards
	board/pru_core/
		source code that is specific to pru_core
		ti-pru-cgt/
			linker.cmd
			makefile (can pass variables to customize the source
				code per core, device, or board)
```

### NON-PRU CODE

non-PRU source code may exist in the top-level directory of the project, or in
a separate subdirectory. Putting the non-PRU source code in a separate
subdirectory makes it easier to expand the project to work with other operating
systems. For example,

```
pru_spi
	firmware
	mcuplus
		code to allow an MCU+ R5F core to interact with PRU SPI firmware
	linux
		code to allow Linux A53 to interact with PRU SPI firmware
```

> [!NOTE]
> If the PRU code must be modified in order to interact with different cores or
> operating systems, it is suggested to create a separate project for each
> version of the PRU firmware.

#### MCU+ source code

MCU+ source code typically follows the same pattern as the PRU source code:

```
mcuplus
        source code that is shared across multiple cores, devices, or boards
        board/core/
		source code that is specific to core
		build-tool/
			linker.cmd
			makefile (can pass variables to customize the source
				code per core, device, or board)
```
