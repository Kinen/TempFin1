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
		x_open_t x_open;			// binding for device open function
		x_ctrl_t x_ctrl;			// ctrl function
		x_gets_t x_gets;			// get string function
		x_getc_t x_getc;			// stdio compatible getc function
		x_putc_t x_putc;			// stdio compatible putc function
		x_flow_t x_flow;			// flow control callback

		// initialization values
		uint32_t baud; 			// baud rate as a long
		uint8_t ucsra_init;		// initialization value for usart control/status register A
		uint8_t ucsrb_init;		// initialization value for usart control/status register B
};

static struct cfgUSART const cfgUsart[] PROGMEM = {
	{
		xio_open_usart,
		xio_ctrl_device,
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

//static int _gets_helper(xioDev_t *d, xioUsart_t *dx);

/*
 *	xio_init_usart() - general purpose USART initialization (shared)
 *
 *	This generic init() requires open() to be performed to complete the device init
 */
void xio_init_usart(void)
{
	for (uint8_t i=0; i<XIO_DEV_USART_COUNT; i++) {
		xio_init_device(XIO_DEV_USART_OFFSET + i,
					   (x_open_t)pgm_read_word(&cfgUsart[i].x_open),
					   (x_ctrl_t)pgm_read_word(&cfgUsart[i].x_ctrl),
					   (x_gets_t)pgm_read_word(&cfgUsart[i].x_gets),
					   (x_getc_t)pgm_read_word(&cfgUsart[i].x_getc),
					   (x_putc_t)pgm_read_word(&cfgUsart[i].x_putc),
					   (x_flow_t)pgm_read_word(&cfgUsart[i].x_flow));
	}
}

/*
 *	xio_open_usart() - general purpose USART open
 *	xio_set_baud_usart() - baud rate setting routine
 *
 *	The open() function assumes that init() has been run previously
 */
FILE *xio_open_usart(const uint8_t dev, const char *addr, const flags_t flags)
{
	// setup pointers and references
	xioDev_t *d = &ds[dev];							// main device struct pointer
	uint8_t idx = dev - XIO_DEV_USART_OFFSET;		// index into USART extended structures
	d->x = (xioUsart_t *)&us[idx];					// bind extended struct to device
	xioUsart_t *dx = (xioUsart_t *)d->x;			// dx is a convenience / efficiency
	memset (dx, 0, sizeof(xioUsart_t));				// zro out the extended struct

	// setup internals
	xio_ctrl_device(d, flags);						// setup control flags	
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

void xio_set_baud_usart(xioUsart_t *dx, const uint32_t baud)
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
	UCSR0B |= (1<<UDRIE0); 								// enable TX interrupts - will keep firing
	return (xio_write_tx_buffer(((xioDev_t *)stream->udata)->x,c));
}

ISR(USART_UDRE_vect)
{
	char c = xio_read_tx_buffer(&USARTx);
	if (c == _FDEV_ERR) UCSR0B &= ~(1<<UDRIE0); else UDR0 = c;
}

/*
 *  xio_getc_usart() - generic char reader for USART devices
 *  USART RX ISR() - This function is hard-wired for the atmega328p config
 *
 *	Compatible with stdio system - may be bound to a FILE handle
 *
 *  Get next character from RX buffer.
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
ISR(USART_RX_vect) 
{ 
	xio_write_rx_buffer(&USARTx, UDR0);
}

int xio_getc_usart(FILE *stream)
{
	char c = xio_read_rx_buffer(((xioDev_t *)stream->udata)->x);

//	xioDev_t *d = (xioDev_t *)stream->udata;		// get device struct pointer
//	d->x_flow(d);										// run the flow control function (USB only)
//	if (d->flag_echo) d->x_putc(c, stdout);				// conditional echo regardless of character
//	if ((c == CR) || (c == LF)) { if (d->flag_linemode) { return('\n');}}

	return (c);
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
int xio_gets_usart(xioDev_t *d, char *buf, const int size)
{
	xioUsart_t *dx =(xioUsart_t *)d->x;			// USART pointer
	char c_out;

	// first time thru initializations
	if (d->flag_in_line == false) {
		d->flag_in_line = true;					// yes, we are busy getting a line
		d->buf = buf;							// bind the output buffer
		d->len = 0;								// zero the buffer count
		d->size = size;							// set the max size of the message
	}
	while (true) {
		if (d->len >= (d->size)-1) {			// size is total count - aka 'num' in fgets()
			d->buf[d->size] = NUL;				// string termination preserves latest char
			return (XIO_BUFFER_FULL);
		}
		if ((c_out = xio_read_rx_buffer(dx)) == _FDEV_ERR) { return (XIO_EAGAIN);}
		if (c_out == LF) {
			d->buf[(d->len)++] = NUL;
			d->flag_in_line = false;			// clear in-line state (reset)
			return (XIO_OK);					// return for end-of-line
		}
		d->buf[(d->len)++] = c_out;				// write character to buffer
	}
}

/*
{
	xioUsart_t *dx =(xioUsart_t *)d->x;			// USART pointer

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
			case (XIO_BUFFER_FULL):	// overrun err
				return (XIO_BUFFER_FULL);
			case (XIO_EOL): 					// got complete line
				return (XIO_OK);
			case (XIO_EAGAIN):					// loop for next character
				break;
		}
	}
	return (XIO_OK);
}

static int _gets_helper(xioDev_t *d, xioUsart_t *dx)
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
//		d->signal = XIO_SIG_EOL;
		return (XIO_BUFFER_FULL);
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
*/

/* Fakeout routines for testing
 *
 *	xio_queue_RX_string_usart() - fake ISR to put a string in the RX buffer
 *	xio_queue_RX_char_usart() - fake ISR to put a char in the RX buffer
 *
 *	String must be NUL terminated but doesn't require a CR or LF
 *	Also has wrappers for USB and RS485
 */
 /*
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
*/
