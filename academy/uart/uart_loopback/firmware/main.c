/*
 * Copyright (C) 2024-2024 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *	* Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 *
 *	* Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the
 *	  distribution.
 *
 *	* Neither the name of Texas Instruments Incorporated nor the names of
 *	  its contributors may be used to endorse or promote products derived
 *	  from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>
#include <pru_uart.h>

/* The FIFO size on the PRU UART is 16 bytes; however, we are (arbitrarily)
 * only going to send 8 at a time */
#define FIFO_SIZE	16
#define MAX_CHARS	8

/* This hostBuffer structure is temporary but stores a data buffer */
struct {
	uint8_t msg; // Not used today
	uint8_t data[FIFO_SIZE];
} hostBuffer;

/* Making this buffer global will force the received data into memory */
uint8_t buffer[MAX_CHARS];

int main(void)
{
	uint8_t tx;
	uint8_t cnt;

	/* FIXME: If modifying this to send data through the pins then PinMuxing
	 * needs to be taken care of prior to running this code.
	 * This is usually done via a GEL file in CCS or by the Linux driver */

	/*
	 * NOTE!!! As of AM62x TRM Rev B:
	 *
	 * - Definitions of PRU UART register field descriptions are missing
	 *
	 *   - The PRU UART hardware in AM335x & AM62x is EXACTLY identical.
	 *     Thus, we can reference the AM335x TRM instead
	 *
	 *   - Detailed register field descriptions can be found in the AM335x
	 *     TRM, Section PRU > Registers > PRU_ICSS_UART Registers
	 *
	 * - PRU UART register bitfields are wrong, for the registers where
	 *   multiple registers share the same base address (RBR, THR, IIR, FCR)
	 *
	 *   - Refer to AM335x TRM for correct bitfields
	 */

	/*** INITIALIZATION ***/

	/* Set up UART to function at 115200 baud - DLL divisor is 104 at 16x oversample
	 * 192MHz / 104 / 16 = ~115200 */
	CT_UART.DIVLSB = 104;
	CT_UART.DIVMSB = 0;
	CT_UART.MODE = 0x0;

	/* Enable Interrupts in UART module. This allows the main thread to poll for
	 * Receive Data Available and Transmit Holding Register Empty */
	CT_UART.INT_EN = 0x7;

	/* If FIFOs are to be used, select desired trigger level and enable
	 * FIFOs by writing to FCR. FIFOEN bit in FCR must be set first before
	 * other bits are configured */
	CT_UART.FCR_bit.FIFOEN = 1;

	/* Enable FIFOs */
	CT_UART.FCR_bit.DMAMODE1 = 1;
	/* RXFIFTL = 0x0 = 1-byte RX FIFO trigger */
	/* Alternative: 8-byte RX FIFO trigger */
	// CT_UART.FCR_bit.RXFIFTL = 0x2;

	/* flush the FIFOs */
	CT_UART.FCR_bit.TXCLR = 1;
	CT_UART.FCR_bit.RXCLR = 1;

	/* Choose desired protocol settings by writing to LCR */
	/* 8-bit word, 1 stop bit, no parity, no break control and no divisor latch */
	CT_UART.LCTR = 3;

	/* Enable loopback for test */
	/*
	 * NOTE!
	 * loopback will prevent the UART from sending data to the output
	 * pins. Remember to disable loopback in the MCR register before
	 * looking for signals on your UART pins.
	 */
	CT_UART.MCTR = 0x10;

	/* Choose desired response to emulation suspend events by configuring
	 * FREE bit and enable UART by setting UTRST and URRST in PWR */
	/* Allow UART to run free, enable UART TX/RX */
	CT_UART.PWR = 0x6001;

	/*** END INITIALIZATION ***/

	/* Priming the 'hostbuffer' with a message */
	hostBuffer.data[0] = 'H';
	hostBuffer.data[1] = 'e';
	hostBuffer.data[2] = 'l';
	hostBuffer.data[3] = 'l';
	hostBuffer.data[4] = 'o';
	hostBuffer.data[5] = '!';
	hostBuffer.data[6] = '\0';

	/*** SEND SOME DATA ***/

	/* Let's send/receive some dummy data */
	for (cnt = 0; cnt < MAX_CHARS; cnt++) {
		/* Load character, ensure it is not string termination */
		if ((tx = hostBuffer.data[cnt]) == '\0')
			break;
		/* Write to the THR data bitfield, as defined in the AM335x TRM */
		CT_UART.THR_bit.DATA = tx;

		/* Because we are doing loopback, wait until LSR.DR == 1
		 * indicating there is data in the RX FIFO */
		while ((CT_UART.LSR1_bit.DR == 0x0));

		/* Read the value from RBR */
		buffer[cnt] = CT_UART.RBR_bit.DATA;

		/* Wait for TX FIFO to be empty */
		while (!((CT_UART.IIR_bit.INTID) == 0x1));
	}

	/*** DONE SENDING DATA ***/

	/* Disable UART before halting */
	CT_UART.PWR = 0x0;

	/* Halt PRU core */
	__halt();
}
