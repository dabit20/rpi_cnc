This directory contains various LinuxCNC modules.

#### L6470 dSpin  driver ####

 - dspin.c
	 Driver for ST Microelectronics L6470 stepper driver chips. Uses bit-banged SPI to communicate with the chips. Configuration of RPi GPIO pin numbers etc. are in the source, not available as LinuxCNC parameters. Install using 'halcompile --install dspin.c'
	 
 - dspin_test.hal
	 Simple HAL test script to verify the module. Use it with 'halrun -I dspin_test.hal'

#### Auxiliary (temperature) controller driver ####

 - auxcontroller.comp
	 Userspace HAL module for the auxiliary temperature/IO controller. Install using 'halcompile --install auxcontroller.comp'
	 
 - auxcontroller_test.hal
	 HAL test script to verify/debug the module. Use it with 'halrun -I auxcontroller_test.hal'

