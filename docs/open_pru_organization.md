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
	Projects sit either directly under academy/ or under a topic subdirectory:
	  academy/<project>              (e.g., getting_started_labs)
	  academy/<topic>/<project>      (e.g., mac/mac_multiply, uart/uart_echo)
	academy/makefile lists entries as <topic>/<project> for topic projects
	and as <project> for top-level projects. There is no intermediate
	per-topic makefile.
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

```
project_name/
    makefile    project-level build orchestration (sets PROJECT_NAME,
                SUPPORTED_PROCESSORS, PRU_DEPENDENCIES, NON_PRU_DEPENDENCIES)
    readme.md
    firmware/   PRU firmware (see PRU CODE below)
    mcuplus/    R5F application (optional; not present on AM62x)
    linux/      Linux A53 application (optional)
```

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
			CCS files: example.projectspec, makefile_projectspec
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
> If only small code changes are required to enable PRU code to interact with
> different cores or operating systems, Instance-specific defines may be added
> to the PRU core's makefile to make the code compile differently. See `DFLAGS`
> in the PRU core's makefile. However, if significant PRU code changes are
> required in order to interact with different cores or operating systems, it is
> suggested to create a separate project for each
> version of the PRU firmware.

#### MCU+ source code

MCU+ source code typically follows the same pattern as the PRU source code:

```
mcuplus
        source code that is shared across multiple cores, devices, or boards
        board/core/
		source code that is specific to core
		example.syscfg (MCU+ SysConfig settings)
		build-tool/
			makefile (can pass variables to customize the source
				code per core, device, or board)
			generated/linker.cmd (generated from example.syscfg)
			CCS files: example.projectspec, makefile_projectspec,
				makefile_ccs_bootimage_gen, syscfg_c.rov.xs
			
```
