subst R: /d
subst R: C:\ti

set CCS_INSTALL_DIR=R:\ccs1030

rem Gmake & Linux tools
path=%CCS_INSTALL_DIR%\xdctools_3_62_00_08_core;%CCS_INSTALL_DIR%\xdctools_3_62_00_08_core\bin;%path%

rem PRU Code Generation Tools
set PRU_CGT_BS=R:\ti-cgt-pru_2.3.3
set PRU_CGT=%PRU_CGT_BS:\=/%