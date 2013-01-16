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

// statics
static int _gets_helper(xioDev *d, xioSpi *dx);

/******************************************************************************
 * SPI CONFIGURATION RECORDS
 ******************************************************************************/
// SPI is setup below for mode3, MSB first. See Atmel Xmega A 8077.doc, page 231

struct cfgSPI {
		x_open x_open;			// see xio.h for typedefs
		x_ctrl x_ctrl;
		x_gets x_gets;
		x_getc x_getc;
		x_putc x_putc;
		fc_func fc_func;

		// initialization values
		uint8_t spcr; 			// initialization value for SPI configuration register
		uint8_t outbits; 		// bits to set as outputs in PORTB
};

static struct cfgSPI const cfgSpi[] PROGMEM = {
	{
		xio_open_spi,			// open function
		xio_ctrl_generic, 		// ctrl function
		xio_gets_spi,			// get string function
		xio_getc_spi,			// stdio getc function
		xio_putc_spi,			// stdio putc function
		xio_fc_null,			// flow control callback

		(1<<SPIE | 1<<SPE | 1<<CPOL | 1<<CPHA),	// mode 3 operation / slave
//		(1<<SPIE | 1<<SPE)						// mode 0 operation / slave
		(1<<DDB4)				// Set SCK, MOSI, SS to input, MISO to output
	}
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
		xio_open_generic(XIO_DEV_SPI_OFFSET + i,
						(x_open)pgm_read_word(&cfgSpi[i].x_open),
						(x_ctrl)pgm_read_word(&cfgSpi[i].x_ctrl),
						(x_gets)pgm_read_word(&cfgSpi[i].x_gets),
						(x_getc)pgm_read_word(&cfgSpi[i].x_getc),
						(x_putc)pgm_read_word(&cfgSpi[i].x_putc),
						(fc_func)pgm_read_word(&cfgSpi[i].fc_func));
	}
}

/*
 *	xio_open_spi() - open a specific SPI device
 */
FILE *xio_open_spi(const uint8_t dev, const char *addr, const CONTROL_T flags)
{
	xioDev *d = &ds[dev];						// setup device struct pointer
	uint8_t idx = dev - XIO_DEV_SPI_OFFSET;
	d->x = &sp[idx];							// setup extended struct pointer
	xioSpi *dx = (xioSpi *)d->x;

	memset (dx, 0, sizeof(xioSpi));				// clear all values
	xio_ctrl_generic(d, flags);					// setup flags

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
 *
 *	Puts RX byte into RX buffer
 *	Transfers next TX byte to SPDR of ETX if none available
 */

ISR(SPI_STC_vect)
{
	// read the incoming character
	uint8_t c_rx = SPDR;

	// stage the next char to transmit on MISO from the TX buffer
	if (SPIx.tx_buf_head != SPIx.tx_buf_tail) {		// TX buffer is not empty
		advance(SPIx.tx_buf_tail, SPI_TX_BUFFER_SIZE-1);	
//		if (--(SPIx.tx_buf_tail) == 0) {			// advance TX tail (TXQ read ptr)
//			SPIx.tx_buf_tail = SPI_TX_BUFFER_SIZE-1;// -1 is OBOE avoidance
//		}
		SPDR = SPIx.tx_buf[SPIx.tx_buf_tail];		// stage char from TX buf to MISO out
	} else {
		SPDR = ETX;
	}
	// detect a collision - next MOSI transmission started before SPDR was written - dang!
//	if (SPSR & 1<<WCOL) {
//		led_on();				// what to do?		
//	}

	// put incoming char into RX buffer
	if ((--SPIx.rx_buf_head) == 0) { 				// adv buffer head with wrap
		SPIx.rx_buf_head = SPI_RX_BUFFER_SIZE-1;
	}
	if (SPIx.rx_buf_head != SPIx.rx_buf_tail) {		// buffer is not full
		SPIx.rx_buf[SPIx.rx_buf_head] = c_rx;		// write char unless full
	} else { 										// buffer-full - toss the incoming character
		if ((++SPIx.rx_buf_head) > RX_BUFFER_SIZE-1) {// reset the head
			SPIx.rx_buf_head = 1;
		}
	}
}

/*
 * xio_putc_spi() - stdio compatible character SPI TX routine for 328 slave
 *
 *	Write a character into the TX buffer for MISO piggyback transmission
 */
int xio_putc_spi(const char c, FILE *stream)
{
//	BUFFER_T temp_tx_head;
	advance(SPIx.tx_buf_head, SPI_TX_BUFFER_SIZE-1);	

	if (SPIx.tx_buf_head == SPIx.tx_buf_tail) {				// buffer is full
		if ((++(SPIx.tx_buf_head)) > SPI_TX_BUFFER_SIZE-1) { // reset the head
			SPIx.tx_buf_head = 1;
		}
		((xioDev *)stream->udata)->signal = XIO_SIG_OVERRUN; // signal a buffer overflow
		return (_FDEV_ERR);
	}
	SPIx.tx_buf[SPIx.tx_buf_head] = c;
	return (XIO_OK);
/*
	advance(SPIx.tx_buf_tail, SPI_TX_BUFFER_SIZE-1);
	if (SPIx.tx_buf_head != SPIx.tx_buf_tail) {	// buffer is not full
		SPIx.tx_buf[SPIx.tx_buf_head] = c;		// write char to buffer
	} else { 									// buffer-full - toss the incoming character
		if ((++(SPIx.tx_buf_head)) > SPI_TX_BUFFER_SIZE-1) { // reset the head
			SPIx.tx_buf_head = 1;
		}
		((xioDev *)stream->udata)->signal = XIO_SIG_OVERRUN; // signal a buffer overflow
		return (_FDEV_ERR);
	}
	return (XIO_OK);
*/
}

/*
 * xio_getc_spi() - stdio compatible character PSI RX routine for 328 slave
 *
 *	All getc_spi() does is read from the RX buffer. If no character is available it 
 *	returns an error. getc() is always non-blocking or it would create a deadlock.
 */
int xio_getc_spi(FILE *stream)
{
	if (SPIx.rx_buf_head == SPIx.rx_buf_tail) {	return(_FDEV_ERR);}	// RX buffer empty
	advance(SPIx.rx_buf_tail, SPI_RX_BUFFER_SIZE-1);
	return(SPIx.rx_buf[SPIx.rx_buf_tail]);
}

/* 
 *	xio_gets_spi() - read a complete line from an SPI device
 * _gets_helper()  - non-blocking character getter for gets
 *
 *	Retains line context across calls - so it can be called multiple times.
 *	Reads as many characters as it can until any of the following is true:
 *
 *	  - Encounters newline indicating a complete line (returns XIO_OK)
 *	  - Encounters and empty buffer and SPI returns ETX indicating there is no data to xfer (return XIO_EAGAIN)
 *	  - Read would cause output buffer overflow (return XIO_BUFFER_FULL)
 *
 *	Note: LINEMODE flag in device struct is ignored. It's ALWAYS LINEMODE here.
 *	Note: CRs are not recognized as NL chars - this function assumes slaves never send CRs. 
 */
int xio_gets_spi(xioDev *d, char *buf, const int size)
{
	xioSpi *dx = (xioSpi *)d->x;				// get SPI device struct pointer

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

static int _gets_helper(xioDev *d, xioSpi *dx)
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

