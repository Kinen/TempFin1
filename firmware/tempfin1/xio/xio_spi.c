/*
 * xio_spi.c	- General purpose SPI device driver for xmega family
 * 				- works with avr-gcc stdio library
 *
 * Part of Kinen project
 *
 * Copyright (c) 2013 Alden S. Hart Jr.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* ---- SPI Protocol ----
 *
 * The SPI master/slave protocol is designed to be as simple as possible. 
 * In short, the master transmits whenever it wants to and the slave returns
 * the next character in its output buffer whenever there's an SPI transfer.
 * No flow control is needed as the master initiates and drives all transfers.
 *
 * More details:
 *
 *	- A "message" is a line of text. Examples of messages are requests from the 
 *		master to a slave, responses to these requests, and asynchronous messages 
 *		(from a slave) that are not tied to a request.
 *
 *		Messages are terminated with a newline (aka NL, LF, line-feed). The 
 *		terminating NL is considered part of the message and should be transmitted.
 *
 *		If multiple NLs are transmitted each trailing NL is interpreted as a blank
 *		message. This is generally not good practice - so watch it.
 *
 *		Carriage return (CR) is not recognized as a newline. A CR in a message is
 *		treated as any other non-special ASCII character.
 *
 *		NULs (0x00) are not transmitted in either direction (e.g. string terminations).
 *		Depending on the master or slave internals, it may convert internal string 
 *		terminating NULs to NLs for transmission. 
 *
 *	- A slave is always in RX state - it must always be able to receive message data (MOSI).
 *
 *	- All SPI transmissions are initiated by the master and are 8 bits long. As the 
 *		slave is receiving the byte on MOSI it should be returning the next character 
 *		in its output buffer on MISO. Note that there is no inherent correlation between
 *		the char (or message) being received from the master and transmitted from the
 *		slave. It's just IO.
 *
 *		If the slave has no data to send it should return ETX (0x03) on MISO. This is 
 *		useful to distinghuish between an "empty" slave and a non-responsive SPI slave or
 *		unpopulated Kinen socket - which would return NULs or possibly 0xFFs.
 *
 *	- The master may poll for message data from the slave by sending STX chars to
 *		the slave. The slave discards all STXs and simply returns output data on these
 *		transfers. Presumably the master would stop polling once it receives an ETX 
 *		from the slave.
 */
#include <stdio.h>						// precursor for xio.h
#include <stdbool.h>					// true and false
#include <string.h>						// for memset
#include <avr/pgmspace.h>				// precursor for xio.h
#include <avr/interrupt.h>
#include <avr/sleep.h>					// needed for blocking TX

#include "xio.h"						// includes for all devices are in here


/******************************************************************************
 * SPI CONFIGURATION RECORDS
 ******************************************************************************/
// SPI is setup below for mode3, MSB first. See Atmel Xmega A 8077.doc, page 231

struct cfgSPI {
		x_open_t x_open;			// binding for device open function
		x_ctrl_t x_ctrl;			// ctrl function
		x_gets_t x_gets;			// get string function
		x_getc_t x_getc;			// stdio compatible getc function
		x_putc_t x_putc;			// stdio compatible putc function
		x_flow_t x_flow;			// flow control callback

		// initialization values
		uint8_t spcr; 			// initialization value for SPI configuration register
		uint8_t outbits; 		// bits to set as outputs in PORTB
};

static struct cfgSPI const cfgSpi[] PROGMEM = {
	{
		xio_open_spi,
		xio_ctrl_device,
		xio_gets_spi,
		xio_getc_spi,
		xio_putc_spi,
		xio_flow_null,

//		(1<<SPIE | 1<<SPE)						// mode 0 operation / slave
		(1<<SPIE | 1<<SPE | 1<<CPOL | 1<<CPHA),	// mode 3 operation / slave
		(1<<DDB4)				// Set SCK, MOSI, SS to input, MISO to output
	},
};

// fast accessors
#define SPI ds[XIO_DEV_SPI]							// device struct accessor
#define SPIx sp[XIO_DEV_SPI - XIO_DEV_SPI_OFFSET]	// SPI extended struct accessor

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*
 *	xio_init_spi() - init entire SPI system
 */
void xio_init_spi(void)
{
	for (uint8_t i=0; i<XIO_DEV_SPI_COUNT; i++) {
		xio_init_device(XIO_DEV_SPI_OFFSET + i,
					   (x_open_t)pgm_read_word(&cfgSpi[i].x_open),
					   (x_ctrl_t)pgm_read_word(&cfgSpi[i].x_ctrl),
					   (x_gets_t)pgm_read_word(&cfgSpi[i].x_gets),
					   (x_getc_t)pgm_read_word(&cfgSpi[i].x_getc),
					   (x_putc_t)pgm_read_word(&cfgSpi[i].x_putc),
					   (x_flow_t)pgm_read_word(&cfgSpi[i].x_flow));
	}
}

/*
 *	xio_open_spi() - open a specific SPI device
 */
FILE *xio_open_spi(const uint8_t dev, const char *addr, const flags_t flags)
{
	xioDev_t *d = &ds[dev];						// setup device struct pointer
	uint8_t idx = dev - XIO_DEV_SPI_OFFSET;
	d->x = &sp[idx];							// setup extended struct pointer
	xioSpi_t *dx = (xioSpi_t *)d->x;

	memset (dx, 0, sizeof(xioSpi_t));			// clear all values
	xio_ctrl_device(d, flags);					// setup control flags	

	// setup internal RX/TX control buffers
	dx->rx_buf_head = 1;		// can't use location 0 in circular buffer
	dx->rx_buf_tail = 1;
	dx->tx_buf_head = 1;
	dx->tx_buf_tail = 1;

	PRR &= ~PRSPI_bm;			// Enable SPI in the power reduction register (system.h)
	SPCR |= (uint8_t)pgm_read_byte(&cfgSpi[idx].spcr);
	DDRB |= (uint8_t)pgm_read_byte(&cfgSpi[idx].outbits);
	return (&d->file);							// return FILE reference
}

/*
 * SPI Slave Interrupt() - interrupts on RX byte received
 *	Put RX byte into RX buffer. Transfer next TX byte to SPDR or ETX if none available
 */
ISR(SPI_STC_vect)
{
	char c = SPDR;								// read the incoming character
	char c_out = xio_read_tx_buffer(&SPIx); 	// stage the next char to transmit on MISO from the TX buffer
	if (c_out ==_FDEV_ERR) SPDR = ETX; else SPDR = c_out;		
	xio_write_rx_buffer(&SPIx,c);				// write incoming char into RX buffer
}

/*
 * xio_putc_spi() - Write a character into the TX buffer for MISO piggyback transmission
 */
int xio_putc_spi(const char c, FILE *stream)
{
	return (xio_write_tx_buffer(((xioDev_t *)stream->udata)->x,c));
}

/*
 * xio_getc_spi() - read char from the RX buffer. Return error if no character available
 */
int xio_getc_spi(FILE *stream)
{
	return (xio_read_rx_buffer(((xioDev_t *)stream->udata)->x));
}

/* 
 *	xio_gets_spi() - read a complete line (message) from an SPI device
 *
 *	Reads from the local RX buffer until it's empty. Retains line context 
 *	across calls - so it can be called multiple times. Reads as many characters 
 *	as it can until any of the following is true:
 *
 *	  - Encounters newline indicating a complete line. Terminate the buffer
 *		but do not write the newlinw into the buffer. Reset line flag. Return XIO_OK.
 *
 *	  - Encounters an empty buffer. Leave in_line. Return XIO_EAGAIN.
 *
 *	  - A successful read would cause output buffer overflow. Return XIO_BUFFER_FULL
 *
 *	Note: LINEMODE flag in device struct is ignored. It's ALWAYS LINEMODE here.
 *	Note: CRs are not recognized as NL chars - master must send LF to terminate a line
 */
int xio_gets_spi(xioDev_t *d, char *buf, const int size)
{
	xioSpi_t *dx = (xioSpi_t *)d->x;			// get SPI device struct pointer
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
		if ((c_out = xio_read_rx_buffer(dx)) == _FDEV_ERR) {
			return (XIO_EAGAIN);
		}
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
	xioSpi_t *dx = (xioSpi_t *)d->x;			// get SPI device struct pointer

	// first time thru initializations
	if (d->flag_in_line == false) {
		d->flag_in_line = true;					// yes, we are busy getting a line
		d->len = 0;								// zero buffer
		d->buf = buf;
		d->size = size;
		d->signal = XIO_SIG_OK;					// reset signal register
	}

	// process chars until done or blocked
	while (true) {
		switch (_gets_helper(d,dx)) {
			case (XIO_BUFFER_EMPTY): 
				return (XIO_EAGAIN); 			// empty condition
			case (XIO_BUFFER_FULL_NON_FATAL): 
				return (XIO_BUFFER_FULL_NON_FATAL);// overrun err
			case (XIO_EOL): 
				return (XIO_OK);				// got complete line
//			case (XIO_EAGAIN): 
//				break;							// break the switch; loop for next character
		}
	}
	return (XIO_ERR);							// never supposed to get here
}

static int _gets_helper(xioDev_t *d, xioSpi_t *dx)
{
	if (d->len >= d->size) {					// handle buffer overruns
		d->buf[d->size] = NUL;					// terminate line (d->size is zero based)
		d->signal = XIO_SIG_EOL;
		return (XIO_BUFFER_FULL_NON_FATAL);
	}
	char c = xio_getc_spi(&d->file);			// get a character or ETX

	if (c == LF) {								// end-of-line condition
		d->buf[(d->len)++] = NUL;
		d->signal = XIO_SIG_EOL;
		d->flag_in_line = false;				// clear in-line state (reset)
		return (XIO_EOL);						// return for end-of-line
	}
	if (c == ETX) {
		return (XIO_BUFFER_EMPTY);				// nothing more to read
	}
	d->buf[(d->len)++] = c;						// write character to buffer
	return (XIO_EAGAIN);
}
*/





