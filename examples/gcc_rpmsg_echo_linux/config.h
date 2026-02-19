
/* These constants are excellently explained in examples/rpmsg_echo_linux/readme.md.
 *
 * Show-off here the GCC's "-mmcu=" option, which among other things
 * automatically defines PRU-instance-specific macros (e.g. __AM62X_PRU0__).
 * Thus instead of manually changing these constants in the Makefile
 * as in the TI CGT example, we can simply define them in this header.
 * An ifdef block is used for each supported PRU core instance.
 *
 *
 * For a list of supported PRU core instances (a.k.a. MCUs), see
 *   https://github.com/dinuxbg/gnuprumcu/blob/master/MCU-LIST.md
 *
 * For list of defined macros, simply open the correspondingly named
 * spec file from the above project. Example:
 *   https://github.com/dinuxbg/gnuprumcu/blob/master/device-specs/am62x.pru0
 */
#if defined(__AM62X_PRU0__)
  #define HOST_INT_BIT 30
  #define TO_ARM_HOST 16
  #define FROM_ARM_HOST 17
  #define CHAN_PORT 30
  #define INTMAP_CHANNEL 0
  #define INTMAP_HOST 0
#elif defined(__AM62X_PRU1__)
  #define HOST_INT_BIT 31
  #define TO_ARM_HOST 18
  #define FROM_ARM_HOST 19
  #define CHAN_PORT 31
  #define INTMAP_CHANNEL 1
  #define INTMAP_HOST 1
#else
  #error "Please define the RPMSG channel properties for your specific PRU instance."
#endif
