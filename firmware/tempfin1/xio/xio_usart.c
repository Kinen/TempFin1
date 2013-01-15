/*
 * xio_usart.c	- General purpose USART device driver for xmega family
 * 				- works with avr-gcc stdio library
 *
 * Part of TinyG project
 *
 * Copyright (c) 2010 - 2013 Alden S. Hart Jr.
 *
 * TinyG is free software: you can redistribute it and/or modify it 
 * under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * TinyG is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 * See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with TinyG  If not, see <http://www.gnu.org/licenses/>.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stdio.h>						// precursor for xio.h
#include <stdbool.h>					// true and false
#include <string.h>						// for memset
#include <avr/pgmspace.h>				// precursor for xio.h.
#include <avr/interrupt.h>
#include <avr/sleep.h>					// needed for blocking character writes

#include "xio.h"						// includes for all devices are in here

/******************************************************************************
 * USART CONFIGURATION RECORDS
 ******************************************************************************/

struct cfgUSART {		
		// function and callback bindings - see xio.h for typedefs
		x_open x_open;			// binding for device open function
		x_ctrl x_ctrl;			//    ctrl function
		x_gets x_gets;			//    get string function
		x_getc x_getc;			//    stdio getc function
		x_putc x_putc;			//    stdio putc function
		fc_func fc_func;		//    flow control callback

		// initialization values
		uint32_t baud; 			// baud rate as a long
		uint8_t ucsra_init;		// initialization value for usart control/status register A
		uint8_t ucsrb_init;		// initialization value for usart control/status register B
};

static struct cfgUSART const cfgUsart[] PROGMEM = {
	{
		xio_open_usart,
		xio_ctrl_generic,
		xio_gets_usart,
		xio_getc_usart,
		xio_putc_usart,
		xio_fc_null,

		115200,					// baud rate
		0,						// turns baud doubler off
		( 1<<RXCIE0 | 1<<TXEN0 | 1<<RXEN0)  // enable recv interrupt, TX and RX
	}
};

// Fast accessors
#define USART ds[XIO_DEV_USART]							// device struct accessor
#define USARTx us[XIO_DEV_USART - XIO_DEV_USART_OFFSET]	// usart extended struct accessor

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

static int _gets_helper(xioDev *d, xioUsart *dx);

/*
 *	xio_init_usart() - general purpose USART initialization (shared)
 *
 *	This generic init() requires open() to be performed to complete the device init
 */
void xio_init_usart(void)
{
	for (uint8_t i=0; i<XIO_DEV_USART_COUNT; i++) {
		xio_open_generic(XIO_DEV_USART_OFFSET + i,
						(x_open)pgm_read_word(&cfgUsart[i].x_open),
						(x_ctrl)pgm_read_word(&cfgUsart[i].x_ctrl),
						(x_gets)pgm_read_word(&cfgUsart[i].x_gets),
						(x_getc)pgm_read_word(&cfgUsart[i].x_getc),
						(x_putc)pgm_read_word(&cfgUsart[i].x_putc),
						(fc_func)pgm_read_word(&cfgUsart[i].fc_func));
	}
}

/*
 *	xio_open_usart() - general purpose USART open
 *	xio_set_baud_usart() - baud rate setting routine
 *
 *	The open() function assumes that init() has been run previously
 */
FILE *xio_open_usart(const uint8_t dev, const char *addr, const CONTROL_T flags)
{
	// setup pointers and references
	xioDev *d = &ds[dev];							// main device struct pointer
	uint8_t idx = dev - XIO_DEV_USART_OFFSET;		// index into USART extended structures
	d->x = (xioUsart *)&us[idx];					// bind extended struct to device
	xioUsart *dx = (xioUsart *)d->x;				// dx is a convenience / efficiency
	memset (dx, 0, sizeof(xioUsart));				// zro out the extended struct

	// setup internals
	xio_ctrl_generic(d, flags);						// setup control flags	
	dx->rx_buf_head = 1;							// can't use location 0 in circular buffer
	dx->rx_buf_tail = 1;
	dx->tx_buf_head = 1;
	dx->tx_buf_tail = 1;

	// setup the hardware
	PRR &= ~PRUSART0_bm;		// Enable the USART in the power reduction register (system.h)
	UCSR0A = (uint8_t)pgm_read_byte(&cfgUsart[idx].ucsra_init);
	UCSR0B = (uint8_t)pgm_read_byte(&cfgUsart[idx].ucsrb_init);
	xio_set_baud_usart(dx,pgm_read_dword(&cfgUsart[idx].baud));

	return (&d->file);								// return stdio FILE reference
}

void xio_set_baud_usart(xioUsart *dx, const uint32_t baud)
{
	UBRR0 = (F_CPU / (8 * baud)) - 1;
	UCSR0A &= ~(1<<U2X0);		// baud doubler off
}

/* 
 * xio_putc_usart() - stdio compatible char writer for usart devices
 * USART TX ISR() - hard-wired for atmega328p 
 */
int xio_putc_usart(const char c, FILE *stream)
{
	BUFFER_T next_tx_buf_head = USARTx.tx_buf_head-1;	// set next head while leaving current one alone
	if (next_tx_buf_head == 0)
		next_tx_buf_head = TX_BUFFER_SIZE-1; 			// detect wrap and adjust; -1 avoids off-by-one
	UCSR0B |= (1<<UDRIE0); 								// enable TX interrupts - will keep firing
	while (next_tx_buf_head == USARTx.tx_buf_tail);		// loop until the buffer has space
	USARTx.tx_buf_head = next_tx_buf_head;				// accept next buffer head
	USARTx.tx_buf[USARTx.tx_buf_head] = c;				// write char to buffer
	return (XIO_OK);
}

ISR(USART_UDRE_vect)
{
	if (USARTx.tx_buf_head != USARTx.tx_buf_tail) {		// buffer has data
		if (--USARTx.tx_buf_tail == 0) 					// advance tail and wrap 
			USARTx.tx_buf_tail = TX_BUFFER_SIZE-1;
		UDR0 = USARTx.tx_buf[USARTx.tx_buf_tail];		// Send a byte from the buffer	
	} else {
		UCSR0B &= ~(1<<UDRIE0);							// turn off interrupts
	}
}

/*
 *  xio_getc_usart() - generic char reader for USART devices
 *  USART RX ISR() - This function is hard-wired for the atmega328p config
 *
 *	Compatible with stdio system - may be bound to a FILE handle
 *
 *  Get next character from RX buffer.
 *  See https://www.synthetos.com/wiki/index.php?title=Projects:TinyG-Module-Details#Notes_on_Circular_Buffers
 *  for a discussion of how the circular buffers work
 *
 *	This routine returns a single character from the RX buffer to the caller.
 *	It's typically called by fgets() and is useful for single-threaded IO cases.
 *	Cases with multiple concurrent IO streams may want to use the gets() function
 *	which is incompatible with the stdio system. 
 *
 *  Flags that affect behavior:
 *
 *  BLOCKING behaviors
 *	 	- execute blocking or non-blocking read depending on controls
 *		- return character or -1 & XIO_SIG_WOULDBLOCK if non-blocking
 *		- return character or sleep() if blocking
 *
 *  ECHO behaviors
 *		- if ECHO is enabled echo character to stdout
 *		- echo all line termination chars as newlines ('\n')
 *		- Note: putc is responsible for expanding newlines to <cr><lf> if needed
 */
int xio_getc_usart(FILE *stream)
{
	xioDev *d = (xioDev *)stream->udata;			// get device struct pointer
	xioUsart *dx =(xioUsart *)d->x;					// USART pointer
	char c;

	while (dx->rx_buf_head == dx->rx_buf_tail) {	// RX ISR buffer empty
		if (d->flag_block) {
			sleep_mode();
		} else {
			d->signal = XIO_SIG_EAGAIN;
			return(_FDEV_ERR);
		}
	}
	if (--(dx->rx_buf_tail) == 0) {					// advance RX tail (RXQ read ptr)
		dx->rx_buf_tail = RX_BUFFER_SIZE-1;			// -1 avoids off-by-one error (OBOE)
	}
//	d->fc_func(d);									// run the flow control function (USB only)
	c = dx->rx_buf[dx->rx_buf_tail];				// get char from RX buf

	// Triage the input character for handling. This code does not handle deletes
	if (d->flag_echo) d->x_putc(c, stdout);			// conditional echo regardless of character
	if (c > CR) return(c); 							// fast cutout for majority cases
	if ((c == CR) || (c == LF)) {
		if (d->flag_linemode) return('\n');
	}
	return(c);
}

ISR(USART_RX_vect)
{
	uint8_t c = UDR0;

	if ((--USARTx.rx_buf_head) == 0) { 				// adv buffer head with wrap
		USARTx.rx_buf_head = RX_BUFFER_SIZE-1;		// -1 avoids off-by-one error
	}
	if (USARTx.rx_buf_head != USARTx.rx_buf_tail) {	// buffer is not full
		USARTx.rx_buf[USARTx.rx_buf_head] = c;		// write char unless full
	} else { 										// buffer-full - toss the incoming character
		if ((++USARTx.rx_buf_head) > RX_BUFFER_SIZE-1) {// reset the head
			USARTx.rx_buf_head = 1;
		}
	}
}
/* 
 *	xio_gets_usart() - read a complete line from the usart device
 * _gets_helper() 	 - non-blocking character getter for gets
 *
 *	Retains line context across calls - so it can be called multiple times.
 *	Reads as many characters as it can until any of the following is true:
 *
 *	  - RX buffer is empty on entry (return XIO_EAGAIN)
 *	  - no more chars to read from RX buffer (return XIO_EAGAIN)
 *	  - read would cause output buffer overflow (return XIO_BUFFER_FULL)
 *	  - read returns complete line (returns XIO_OK)
 *
 *	Note: LINEMODE flag in device struct is ignored. It's ALWAYS LINEMODE here.
 *	Note: This function assumes ignore CR and ignore LF handled upstream before the RX buffer
 */
int xio_gets_usart(xioDev *d, char *buf, const int size)
{
	xioUsart *dx =(xioUsart *)d->x;				// USART pointer

	if (d->flag_in_line == false) {				// first time thru initializations
		d->flag_in_line = true;					// yes, we are busy getting a line
		d->len = 0;								// zero buffer
		d->buf = buf;
		d->size = size;
		d->signal = XIO_SIG_OK;					// reset signal register
	}
	while (true) {
		switch (_gets_helper(d,dx)) {
			case (XIO_BUFFER_EMPTY):			// empty condition
				return (XIO_EAGAIN);
			case (XIO_BUFFER_FULL_NON_FATAL):	// overrun err
				return (XIO_BUFFER_FULL_NON_FATAL);
			case (XIO_EOL): 					// got complete line
				return (XIO_OK);
			case (XIO_EAGAIN):					// loop for next character
				break;
		}
	}
	return (XIO_OK);
}

static int _gets_helper(xioDev *d, xioUsart *dx)
{
	char c = NUL;

	if (dx->rx_buf_head == dx->rx_buf_tail) {	// RX ISR buffer empty
		return(XIO_BUFFER_EMPTY);				// stop reading
	}
	if (--(dx->rx_buf_tail) == 0) {				// advance RX tail (RX q read ptr)
		dx->rx_buf_tail = RX_BUFFER_SIZE-1;		// -1 avoids off-by-one (OBOE)
	}
//	d->fc_func(d);								// run the flow control callback
	c = dx->rx_buf[dx->rx_buf_tail];			// get char from RX Q
	if (d->flag_echo) d->x_putc(c, stdout);		// conditional echo regardless of character

	if (d->len >= d->size) {					// handle buffer overruns
		d->buf[d->size] = NUL;					// terminate line (d->size is zero based)
		d->signal = XIO_SIG_EOL;
		return (XIO_BUFFER_FULL_NON_FATAL);
	}
	if ((c == CR) || (c == LF)) {				// handle CR, LF termination
		d->buf[(d->len)++] = NUL;
		d->signal = XIO_SIG_EOL;
		d->flag_in_line = false;				// clear in-line state (reset)
		return (XIO_EOL);						// return for end-of-line
	}
	d->buf[(d->len)++] = c;						// write character to buffer
	return (XIO_EAGAIN);
}


/* Fakeout routines for testing
 *
 *	xio_queue_RX_string_usart() - fake ISR to put a string in the RX buffer
 *	xio_queue_RX_char_usart() - fake ISR to put a char in the RX buffer
 *
 *	String must be NUL terminated but doesn't require a CR or LF
 *	Also has wrappers for USB and RS485
 */
void xio_queue_RX_string_usart(const uint8_t dev, const char *buf)
{
	uint8_t i=0;
	while (buf[i] != NUL) {
		xio_queue_RX_char_usart(dev, buf[i++]);
	}
}

void xio_queue_RX_char_usart(const uint8_t dev, const char c)
{
	xioDev *d = &ds[dev];						// init device struct pointer
	xioUsart *dx =(xioUsart *)d->x;				// USART pointer

	if ((--dx->rx_buf_head) == 0) { 			// wrap condition
		dx->rx_buf_head = RX_BUFFER_SIZE-1;		// -1 avoids the off-by-one err
	}
	if (dx->rx_buf_head != dx->rx_buf_tail) {	// write char unless buffer full
		dx->rx_buf[dx->rx_buf_head] = c;		// FAKE INPUT DATA
		return;
	}
	// buffer-full handling
	if ((++dx->rx_buf_head) > RX_BUFFER_SIZE-1) { // reset the head
		dx->rx_buf_head = 1;
	}
}
