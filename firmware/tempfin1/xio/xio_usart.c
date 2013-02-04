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
#include <stdio.h>					// precursor for xio.h
#include <stdbool.h>				// true and false
#include <avr/interrupt.h>
#include "xio.h"					// includes for all devices are in here

// allocate and initialize USART structs
xioUsartRX_t usart_rx = { USART_RX_BUFFER_SIZE-1,1,1 };
xioUsartTX_t usart_tx = { USART_TX_BUFFER_SIZE-1,1,1 };
xioDev_t usart0 = {
		XIO_DEV_USART,
		xio_open_usart,
		xio_ctrl_device,
		xio_gets_device,
		xio_getc_usart,
		xio_putc_usart,
		xio_null,
		(xioBuf_t *)&usart_rx,
		(xioBuf_t *)&usart_tx,		// unnecessary to initialize the rest of the struct 
};

// Fast accessors
#define USARTrx ds[XIO_DEV_USART]->rx
#define USARTtx ds[XIO_DEV_USART]->tx

/*
 *	xio_init_usart() - general purpose USART initialization (shared)
 *					   requires open() to be performed to complete the device init
 */
xioDev_t *xio_init_usart(uint8_t dev)
{
	usart0.dev = dev;	// overwite the structure initialization value in case it was wrong
	return (&usart0);
}

/*
 *	xio_open_usart() - general purpose USART open
 *	xio_set_baud_usart() - baud rate setting routine
 *
 *	The open() function assumes that init() has been run previously
 */
FILE *xio_open_usart(const uint8_t dev, const char *addr, const flags_t flags)
{
	xioDev_t *d = ds[dev];						// convenience device struct pointer
	xio_reset_working_flags(d);
	xio_ctrl_device(d, flags);					// setup control flags
	d->rx->wr = 1;								// can't use location 0 in circular buffer
	d->rx->rd = 1;
	d->tx->wr = 1;
	d->tx->rd = 1;

	// setup the hardware
	PRR &= ~PRUSART0_bm;		// Enable the USART in the power reduction register (system.h)
	UCSR0A = USART_BAUD_DOUBLER;
	UCSR0B = USART_ENABLE_FLAGS;
	xio_set_baud_usart(d, USART_BAUD_RATE);

	// setup stdio stream structure
	fdev_setup_stream(&d->file, d->x_putc, d->x_getc, _FDEV_SETUP_RW);
	fdev_set_udata(&d->file, d);		// reference yourself for udata 
	return (&d->file);								// return stdio FILE reference
}

void xio_set_baud_usart(xioDev_t *d, const uint32_t baud)
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
	int status = xio_write_buffer(((xioDev_t *)stream->udata)->tx, c);
	UCSR0B |= (1<<UDRIE0); 	// enable TX interrupts - they will keep firing
	return (status);
}

ISR(USART_UDRE_vect)
{
	int c = xio_read_buffer(USARTtx);
	if (c == _FDEV_ERR) {
		UCSR0B &= ~(1<<UDRIE0); 	// disable interrupts
	} else {
		UDR0 = (char)c;				// write char to USART xmit register
	}
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
	xio_write_buffer(USARTrx, UDR0);
}

int xio_getc_usart(FILE *stream)
{
	char c = xio_read_buffer(((xioDev_t *)stream->udata)->rx);
	xioDev_t *d = (xioDev_t *)stream->udata;		// get device struct pointer
	d->x_flow(d);									// run the flow control function (USB only)
	if (d->flag_echo) d->x_putc(c, stdout);			// conditional echo regardless of character
	if ((c == CR) || (c == LF)) { if (d->flag_linemode) { return('\n');}}
	return (c);
}

/* 
 *	xio_gets_usart() - read a complete line from the usart device
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
/*
int xio_gets_usart(xioDev_t *d, char *buf, const int size)
{
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
		if ((c_out = xio_read_buffer(d->rx)) == _FDEV_ERR) { return (XIO_EAGAIN);}
		if (c_out == LF) {
			d->buf[(d->len)++] = NUL;
			d->flag_in_line = false;			// clear in-line state (reset)
			return (XIO_OK);					// return for end-of-line
		}
		d->buf[(d->len)++] = c_out;				// write character to buffer
	}
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
