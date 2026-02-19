
/* The GNU toolchain for PRU declares all MCU-specific
 * constants in a single header.
 *
 * Provide this pru_intc.h file, in order to avoid needless
 * changes TI-CGT-specific header with the same filename.
 * The RPMSG source library, whose source is shared between
 * TI CGT and GCC examples, includes this header. */
#include <pru/io.h>
