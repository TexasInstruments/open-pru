<div align="center">

<img src="https://upload.wikimedia.org/wikipedia/commons/b/ba/TexasInstruments-Logo.svg" width="150"><br/>

# Creating a New Project in the OPEN PRU Repo

</div>

delete all processors, boards, and cores that are not used in your project from:
1a) project/makefile
1b) project/boards
1c) project/firmware/boards

find and replace old_project_name with new_project_name
e.g., in the Linux terminal
$ cd project
$ grep -r old_project_name
check that if you do a bulk find/replace, you will not mess up any variables
$ grep -rli 'old_project_name' | xargs -i@ sed -i 's/old_project_name/new_project_name/g' @

make any other updates needed to project/makefile

add project to open-pru/examples/makefile so that it builds from a top-level build

### Make infrastructure

Your project should build from a top-level make command. Like this:

```
$ cd /path/to/open-pru
$ make -s
```

For details about how to set up the makefile infrastructure for your project,
refer to
[Creating a New Project in the OPEN PRU Repo](./open_pru_create_new_project.md) 

So what makefile infrastructure do we need in order to build?

Let's say we are adding an example with MCU+ code to examples/new_project. This
example runs on AM243x and AM64x.

#### Which files need to be modified?

We can use the empty example as a template.

open-pru/makefile
* "make" is the same as "make all". The "make all" command passes a "make"
  command to all subfolders listed in SUBDIRS
* no modifications needed

open-pru/examples/makefile
* "make all" passes a "make" command to all subfolders listed in SUBDIRS
* We need to add new_project to this file
* If this project only had PRU code, then we would list the project under
  SUBDIRS :=
* However, this project also has MCU+ code. Add new_project to
  SUBDIRS_MCUPLUS := .
  Now a top-level make command will only build the project if BUILD_MCUPLUS = y
  in imports.mak

open-pru/examples/new_project/makefile
* Add a makefile in this directory. Let's start by copying the file
  examples/empty/makefile
* We will modify this file in the next section

Finally, there should be a makefile for every processor that is getting
programmed in the project. Typical locations for these makefiles include:

* open-pru/examples/new_project/firmware/board/pru-core/build-tool/makefile
* open-pru/examples/new_project/board/non-pru-core/build-tool/makefile

#### Modifying the project makefile

Now let's modify open-pru/examples/new_project/makefile.

Update the "project information" section:
* PROJECT_NAME := new_project
* SUPPORTED_PROCESSORS := am243x am64x

"make all" builds the target that matches $(DEVICE) in imports.mak. So:
* "make" with $(DEVICE) = am261x does nothing
* "make" with $(DEVICE) = am243x runs all commands under the target called
  am243x:
* etc

First, remove all targets that do not apply for this project. Delete
sections "am261x:", "am263px:", etc.

Next, make sure that the $(MAKE) commands under am243x: and am64x: have actual
targets. Let's say that we are only building for ICSSG0_PRU0, ICSSG1_PRU0, and
R5F0_0 on the EVM boards. Then we would update the makefile like this:

```
am243x:
# am243x-evm
        $(MAKE) -C firmware/am243x-evm/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
        $(MAKE) -C firmware/am243x-evm/icss_g1_pru0_fw/ti-pru-cgt $(ARGUMENTS)
        $(MAKE) -C am243x-evm/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)

am64x:
# am64x-evm
        $(MAKE) -C firmware/am64x-evm/icss_g0_pru0_fw/ti-pru-cgt $(ARGUMENTS)
        $(MAKE) -C firmware/am64x-evm/icss_g1_pru0_fw/ti-pru-cgt $(ARGUMENTS)
        $(MAKE) -C am64x-evm/r5fss0-0_freertos/ti-arm-clang $(ARGUMENTS)
```

#### Testing the make infrastructure

First, make sure that the project make file builds from the project directory.
Then, check that the project make builds from the top-level directory.

```
$ cd /path/to/open-pru/examples/new_project

// remove files from previous builds
$ make -s clean

// view all build output to make sure there are no sneaky errors
// in order to see all the prints, do not use -s
$ make

// now test the top-level build
$ make -s clean
$ cd /path/to/open-pru
$ make -s
```
